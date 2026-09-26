#pragma once
#include "esp_err.h"
#include <stdint.h>
esp_err_t sd_storage_init(void);
uint64_t sd_storage_total_bytes(void);
uint64_t sd_storage_free_bytes(void);
