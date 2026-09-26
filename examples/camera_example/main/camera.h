#pragma once
#include "esp_err.h"
#include "esp_camera.h"
esp_err_t camera_app_init(void);
camera_fb_t *camera_app_capture(void);
