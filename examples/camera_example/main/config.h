#pragma once
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define STATIC_IP       "192.168.1.50"
#define GATEWAY         "192.168.1.1"
#define SUBNET_MASK     "255.255.255.0"
#define PRIMARY_DNS     "1.1.1.1"
#define SECONDARY_DNS   "8.8.8.8"
#define WEB_SERVER_PORT 80
#define CAMERA_FRAME_SIZE   FRAMESIZE_VGA
#define CAMERA_JPEG_QUALITY 12
#define CAMERA_FB_COUNT     2
#define RECORDING_ENABLED_ON_BOOT 1
#define RECORDING_FPS             8
#define RECORDING_SEGMENT_SECONDS 300
#define RECORDING_DIRECTORY       "/sdcard/recordings"
#define RECORDING_PREFIX          "cam_"
#define RECORDING_EXTENSION       ".mjpg"
#define SD_MIN_FREE_BYTES         (100ULL * 1024ULL * 1024ULL)
#define WIFI_RECONNECT_DELAY_MS  5000
