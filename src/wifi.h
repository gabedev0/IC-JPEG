#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"

#define WIFI_AP_SSID     "IC-JPEG-CAM"
#define WIFI_AP_PASS     "icjpeg"
#define WIFI_AP_CHANNEL  1
#define WIFI_AP_MAX_CONN 4

#define WIFI_STA_SSID    "SSID"
#define WIFI_STA_PASS    "PASS"

/**
 * Initialize WiFi in AP+STA mode.
 * AP: 192.168.4.1, STA: attempts connection to configured network.
 * Requires NVS (initialized internally).
 */
esp_err_t wifi_init_apsta(void);

#endif /* WIFI_H */
