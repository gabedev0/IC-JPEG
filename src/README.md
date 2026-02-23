# Firmware ESP32-CAM — Exemplo de Uso da `libimage`

Este diretório contém o firmware para **ESP32-CAM AI Thinker**, que serve como plataforma de testes embarcada para a biblioteca [`libimage`](../libimage/). O firmware implementa um web server HTTP que permite comprimir/descomprimir imagens usando os 4 métodos DCT, tanto com a câmera do próprio módulo quanto com imagens enviadas de um PC.

> **Nota:** A biblioteca `libimage` é genérica e portátil. Este firmware é apenas um exemplo de aplicação.
> Para usar a `libimage` em outro projeto embarcado, basta copiar `libimage/src/` e `libimage/include/`.

---

## 📋 Hardware

| Item | Especificação |
|------|---------------|
| **Módulo** | AI Thinker ESP32-CAM |
| **CPU** | ESP32 @ 240 MHz, dual-core |
| **PSRAM** | 8 MB (a `libimage` aloca buffers aqui via `heap_caps_calloc`) |
| **Câmera** | OV2640, configurada em QVGA 320×240 |
| **Framework** | ESP-IDF 5.5.0 via PlatformIO (espressif32) |
| **Partição** | `huge_app.csv` (3 MB app) |

---

## 📂 Estrutura dos Arquivos

| Arquivo | Descrição |
|---------|-----------|
| `main.c` | Ponto de entrada: inicializa WiFi, câmera e webserver |
| `webserver.c` / `webserver.h` | Endpoints HTTP (Método A e B), serve página web |
| `wifi.c` / `wifi.h` | Configuração WiFi dual (AP + STA) |
| `metrics.c` / `metrics.h` | Cálculo de PSNR e bitrate no ESP32 |
| `index_html.h` | Página HTML embarcada (interface web) |
| `CMakeLists.txt` | Build do componente `main` (ESP-IDF) |

---

## 📡 Modos de Operação

### Método A — Compressão + Descompressão no ESP32

O ESP32 executa o pipeline completo (compress → decompress) e retorna a imagem BMP reconstruída.

```
PC ──[BMP]──► ESP32 ──[compress]──[decompress]──[BMP]──► PC
```

**Endpoints:**
- `GET /capture?method=loeffler&quality=2.0` — Usa imagem da câmera
- `POST /process?method=loeffler&quality=2.0` — Usa imagem enviada pelo PC

### Método B — Compressão no ESP32, Descompressão no PC

O ESP32 comprime e envia apenas os coeficientes quantizados (payload int16). O PC executa a descompressão usando a `libimage` via ctypes.

```
PC ──[BMP]──► ESP32 ──[compress]──[int16 coeffs]──► PC ──[decompress]──► BMP
```

**Endpoints:**
- `GET /compressed?method=matrix&quality=1.0` — Usa imagem da câmera
- `POST /process_compressed?method=matrix&quality=1.0` — Usa imagem enviada pelo PC

**Vantagem:** Elimina o tempo de descompressão no ESP32 (~1.5s para Loeffler).

---

## 🌐 Configuração WiFi

O firmware opera em modo **AP + STA** simultaneamente:

| Modo | SSID | Senha | IP |
|------|------|-------|----|
| **AP** (Access Point) | `ESP32-CAM` | `12345678` | `192.168.4.1` |
| **STA** (Station) | Configurável em `wifi.c` | — | `10.0.0.196` (fixo) |

Para alterar a rede STA, edite as constantes em [wifi.c](wifi.c):

```c
#define WIFI_STA_SSID "sua_rede"
#define WIFI_STA_PASS "sua_senha"
```

---

## 🔧 Build e Upload

### Pré-requisitos

- [PlatformIO](https://platformio.org/) instalado (CLI ou extensão VS Code)
- ESP32-CAM conectada via USB-Serial (FTDI ou similar)

### Comandos

```bash
# Na raiz do projeto (onde está platformio.ini)
pio run                    # Compilar firmware
pio run -t upload          # Upload para ESP32-CAM
pio device monitor         # Monitor serial (115200 baud)
pio run -t upload && pio device monitor  # Upload + monitor
```

### Configuração PlatformIO

O arquivo [`platformio.ini`](../platformio.ini) configura:

```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = espidf
board_build.partitions = huge_app.csv
monitor_speed = 115200
```

---

## 🖥️ Receptor PC — `pc_receiver.py`

O script [`pc_receiver.py`](../pc_receiver.py) na raiz do projeto é o cliente que interage com o firmware.

### Uso Básico

```bash
# Captura ao vivo da câmera (Método A)
python pc_receiver.py --method loeffler --quality 2.0

# Enviar imagem conhecida (Método A, todos os métodos DCT)
python pc_receiver.py --image imgs/monarch.bmp --all-methods --quality 1.0

# Método B (coeficientes quantizados)
python pc_receiver.py --method-b --method matrix --quality 1.0

# Todos os métodos, Método A e B, com imagem conhecida
python pc_receiver.py --image imgs/monarch.bmp --all-methods --method-b --quality 2.0
```

### Opções

| Flag | Descrição |
|------|-----------|
| `--method` | DCT: `loeffler`, `matrix`, `approximate`, `identity` |
| `--quality` | Fator k de quantização (1.0 = alta, 8.0 = baixa) |
| `--image` | Envia imagem BMP do PC (em vez de usar câmera) |
| `--method-b` | Usa Método B (decompress no PC) |
| `--all-methods` | Testa todos os 4 métodos DCT |
| `--esp-ip` | IP do ESP32 (padrão: `10.0.0.196`) |

---

## 📊 Benchmarks — monarch 320×240, k=2.0

Resultados obtidos via `pc_receiver.py --image imgs/monarch.bmp`:

| Método | PSNR (dB) | Bitrate (bpp) | Compressão | ESP32 compress | ESP32 decompress | PC compress | PC decompress |
|--------|:---------:|:-------------:|:----------:|:--------------:|:----------------:|:-----------:|:-------------:|
| **Loeffler** | 27.88 | 0.713 | 33.7:1 | 2.582 s | 1.567 s | 5.4 ms | 2.0 ms |
| **Matrix** | 27.88 | 0.713 | 33.7:1 | 2.841 s | 1.892 s | 6.5 ms | 2.4 ms |
| **Approx** | 26.09 | 0.743 | 32.3:1 | 2.189 s | 1.144 s | 3.0 ms | 1.1 ms |
| **Identity** | 43.89 | 7.998 | 3.0:1 | 2.089 s | 1.010 s | 1.8 ms | 0.7 ms |

**Observações:**
- Loeffler é **~10% mais rápido** que Matrix no ESP32 (mesmo PSNR)
- Approx é **~15% mais rápido** que Loeffler, mas ~1.8 dB abaixo em PSNR
- **Método B** elimina o tempo de descompressão no ESP32 (~1.5s), transferindo para o PC (~2ms)
- Identity confirma overhead mínimo do pipeline (PSNR 43.89 = erro apenas de RGB↔YCbCr)

---

## 📚 Links

- [Biblioteca libimage](../libimage/) — Código-fonte da biblioteca C
- [README principal](../README.md) — Visão geral do projeto
- [Python (src_py/)](../src_py/) — Implementação Python para análise
- [pc_receiver.py](../pc_receiver.py) — Script receptor PC
