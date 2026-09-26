#include "sd_storage.h"
#include "config.h"
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static sdmmc_card_t *s_card;
static const char *TAG="sd";
esp_err_t sd_storage_init(void){
    esp_vfs_fat_sdmmc_mount_config_t mc={.format_if_mount_failed=false,.max_files=8,.allocation_unit_size=16*1024};
    sdmmc_host_t host=SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot=SDMMC_SLOT_CONFIG_DEFAULT(); slot.width=1;
    esp_err_t r=esp_vfs_fat_sdmmc_mount("/sdcard",&host,&slot,&mc,&s_card);
    if(r!=ESP_OK){ESP_LOGE(TAG,"SD mount failed: %s",esp_err_to_name(r));return r;}
    struct stat st;
    if(stat(RECORDING_DIRECTORY,&st)!=0 && mkdir(RECORDING_DIRECTORY,0775)!=0) return ESP_FAIL;
    ESP_LOGI(TAG,"SD mounted");
    return ESP_OK;
}
uint64_t sd_storage_total_bytes(void){return s_card?(uint64_t)s_card->csd.capacity*512ULL:0;}
uint64_t sd_storage_free_bytes(void){
    FATFS *fs=NULL; DWORD fc=0;
    if(esp_vfs_fat_get_fatfs("/sdcard",&fs)!=ESP_OK||!fs) return 0;
    if(f_getfree("",&fc,&fs)!=FR_OK) return 0;
    return (uint64_t)fc*fs->csize*512ULL;
}
