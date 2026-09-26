#pragma once
#include "esp_err.h"
#include <stdbool.h>
esp_err_t recorder_init(void);
void recorder_start(void);
void recorder_stop(void);
bool recorder_is_recording(void);
