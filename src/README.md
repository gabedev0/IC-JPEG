# ESP32-CAM Firmware — `libimage` Usage Example

This directory contains the firmware for **ESP32-CAM AI Thinker**, which serves as an embedded test platform for the [`libimage`](../libimage/) library. The firmware implements an HTTP web server that allows compressing/decompressing images using all 4 DCT methods, both with the module's own camera and with images sent from a PC.

> **Note:** The `libimage` library is generic and portable. This firmware is just an example application.
> To use `libimage` in another embedded project, simply copy `libimage/src/` and `libimage/include/`.

---

## 📋 Hardware

| Item | Specification |
|------|---------------|
| **Module** | AI Thinker ESP32-CAM |
| **CPU** | ESP32 @ 240 MHz, dual-core |
| **PSRAM** | 8 MB (`libimage` allocates buffers here via `heap_caps_calloc`) |
| **Camera** | OV2640, configured at QVGA 320×240 |
| **Framework** | ESP-IDF 5.5.0 via PlatformIO (espressif32) |
| **Partition** | `huge_app.csv` (3 MB app) |

---

## 📂 File Structure

| File | Description |
|------|-------------|
| `main.c` | Entry point: initializes WiFi, camera, and webserver |
| `webserver.c` / `webserver.h` | HTTP endpoints (Method A and B), serves web page |
| `wifi.c` / `wifi.h` | Dual WiFi configuration (AP + STA) |
| `metrics.c` / `metrics.h` | PSNR and bitrate calculation on ESP32 |
| `index_html.h` | Embedded HTML page (web interface) |
| `CMakeLists.txt` | `main` component build (ESP-IDF) |

---

## 📡 Operation Modes

### Method A — Compression + Decompression on ESP32

The ESP32 runs the full pipeline (compress → decompress) and returns the reconstructed BMP image.

```
PC ──[BMP]──► ESP32 ──[compress]──[decompress]──[BMP]──► PC
```

**Endpoints:**
- `GET /capture?method=loeffler&quality=2.0` — Uses camera image
- `POST /process?method=loeffler&quality=2.0` — Uses image sent from PC

### Method B — Compression on ESP32, Decompression on PC

The ESP32 compresses and sends only the quantized coefficients (int16 payload). The PC performs decompression using `libimage` via ctypes.

```
PC ──[BMP]──► ESP32 ──[compress]──[int16 coeffs]──► PC ──[decompress]──► BMP
```

**Endpoints:**
- `GET /compressed?method=matrix&quality=1.0` — Uses camera image
- `POST /process_compressed?method=matrix&quality=1.0` — Uses image sent from PC

**Advantage:** Eliminates decompression time on ESP32 (~1.5s for Loeffler).

---

## 🌐 WiFi Configuration

The firmware operates in **AP + STA** mode simultaneously:

| Mode | SSID | Password | IP |
|------|------|----------|----|
| **AP** (Access Point) | `ESP32-CAM` | `12345678` | `192.168.4.1` |
| **STA** (Station) | Configurable in `wifi.c` | — | `10.0.0.196` (fixed) |

To change the STA network, edit the constants in [wifi.c](wifi.c):

```c
#define WIFI_STA_SSID "your_network"
#define WIFI_STA_PASS "your_password"
```

---

## 🔧 Build & Upload

### Prerequisites

- [PlatformIO](https://platformio.org/) installed (CLI or VS Code extension)
- ESP32-CAM connected via USB-Serial (FTDI or similar)

### Commands

```bash
# At the project root (where platformio.ini is located)
pio run                    # Build firmware
pio run -t upload          # Upload to ESP32-CAM
pio device monitor         # Serial monitor (115200 baud)
pio run -t upload && pio device monitor  # Upload + monitor
```

### PlatformIO Configuration

The [`platformio.ini`](../platformio.ini) file configures:

```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = espidf
board_build.partitions = huge_app.csv
monitor_speed = 115200
```

---

## 🖥️ PC Receiver — `pc_receiver.py`

The [`pc_receiver.py`](../pc_receiver.py) script at the project root is the client that interacts with the firmware.

### Basic Usage

```bash
# Live camera capture (Method A)
python pc_receiver.py --method loeffler --quality 2.0

# Send known image (Method A, all DCT methods)
python pc_receiver.py --image imgs/monarch.bmp --all-methods --quality 1.0

# Method B (quantized coefficients)
python pc_receiver.py --method-b --method matrix --quality 1.0

# All methods, Method A and B, with known image
python pc_receiver.py --image imgs/monarch.bmp --all-methods --method-b --quality 2.0
```

### Options

| Flag | Description |
|------|-------------|
| `--method` | DCT: `loeffler`, `matrix`, `approximate`, `identity` |
| `--quality` | Quantization k-factor (1.0 = high, 8.0 = low) |
| `--image` | Send BMP image from PC (instead of using camera) |
| `--method-b` | Use Method B (decompress on PC) |
| `--all-methods` | Test all 4 DCT methods |
| `--esp-ip` | ESP32 IP (default: `10.0.0.196`) |

---

## 📊 Benchmarks — monarch 320×240, k=2.0

Results obtained via `pc_receiver.py --image imgs/monarch.bmp`:

| Method | PSNR (dB) | Bitrate (bpp) | Compression | ESP32 compress | ESP32 decompress | PC compress | PC decompress |
|--------|:---------:|:-------------:|:-----------:|:--------------:|:----------------:|:-----------:|:-------------:|
| **Loeffler** | 27.88 | 0.713 | 33.7:1 | 2.582 s | 1.567 s | 5.4 ms | 2.0 ms |
| **Matrix** | 27.88 | 0.713 | 33.7:1 | 2.841 s | 1.892 s | 6.5 ms | 2.4 ms |
| **Approx** | 26.09 | 0.743 | 32.3:1 | 2.189 s | 1.144 s | 3.0 ms | 1.1 ms |
| **Identity** | 43.89 | 7.998 | 3.0:1 | 2.089 s | 1.010 s | 1.8 ms | 0.7 ms |

**Notes:**
- Loeffler is **~10% faster** than Matrix on ESP32 (same PSNR)
- Approx is **~15% faster** than Loeffler, but ~1.8 dB below in PSNR
- **Method B** eliminates decompression time on ESP32 (~1.5s), transferring it to PC (~2ms)
- Identity confirms minimal pipeline overhead (PSNR 43.89 = RGB↔YCbCr error only)

---

## 📚 Links

- [libimage library](../libimage/) — C library source code
- [Main README](../README.md) — Project overview
- [Python (src_py/)](../src_py/) — Python implementation for analysis
- [pc_receiver.py](../pc_receiver.py) — PC receiver script

---
---

### 🇧🇷 Versão em Português

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
