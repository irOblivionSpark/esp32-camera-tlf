# AI-Thinker ESP32-CAM 24/7 Camera

Edit main/config.h for Wi-Fi SSID/password and static IP/gateway/DNS.

The target is AI-Thinker ESP32-CAM with OV2640. Firmware uses Wi-Fi STA only (no AP), a static IPv4 address, HTTP web panel, live MJPEG stream, and MicroSD recording.

Default recording is enabled at boot, 8 FPS, VGA JPEG, split into 5-minute .mjpg segments.

Build:
idf.py set-target esp32
idf.py build

Flash:
flash_windows.bat COM5

Open:
http://STATIC_IP/
