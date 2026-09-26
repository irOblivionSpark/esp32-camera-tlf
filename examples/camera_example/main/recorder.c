#include "recorder.h"
#include "camera.h"
#include "config.h"
#include "sd_storage.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG="recorder";
static volatile bool s_recording=false;
static TaskHandle_t s_task=NULL;
static FILE *s_file=NULL;
static int64_t s_segment_start;
static unsigned s_seq;

static bool open_segment(void){
    if(sd_storage_free_bytes()<SD_MIN_FREE_BYTES) return false;
    time_t now; struct tm t; char path[180]; time(&now); localtime_r(&now,&t);
    if(t.tm_year<120) snprintf(path,sizeof(path),RECORDING_DIRECTORY "/%s%06u%s",RECORDING_PREFIX,s_seq++,RECORDING_EXTENSION);
    else snprintf(path,sizeof(path),RECORDING_DIRECTORY "/%s%04d%02d%02d_%02d%02d%02d_%u%s",RECORDING_PREFIX,t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec,s_seq++,RECORDING_EXTENSION);
    s_file=fopen(path,"wb"); if(!s_file){ESP_LOGE(TAG,"Cannot open %s",path);return false;}
    s_segment_start=esp_timer_get_time()/1000; ESP_LOGI(TAG,"Recording %s",path); return true;
}
static void close_segment(void){if(s_file){fflush(s_file);fclose(s_file);s_file=NULL;}}
static void task(void *arg){
    const TickType_t delay=pdMS_TO_TICKS(1000/RECORDING_FPS);
    for(;;){
        if(!s_recording){vTaskDelay(pdMS_TO_TICKS(100));continue;}
        if(!s_file&&!open_segment()){s_recording=false;continue;}
        if((esp_timer_get_time()/1000-s_segment_start)>=RECORDING_SEGMENT_SECONDS*1000LL){
            close_segment(); if(!open_segment()){s_recording=false;continue;}
        }
        camera_fb_t *fb=camera_app_capture();
        if(fb){
            if(s_file&&fb->format==PIXFORMAT_JPEG){
                if(fwrite(fb->buf,1,fb->len,s_file)!=fb->len){close_segment();s_recording=false;}
                else fflush(s_file);
            }
            esp_camera_fb_return(fb);
        }
        vTaskDelay(delay);
    }
}
esp_err_t recorder_init(void){
    if(xTaskCreatePinnedToCore(task,"recorder",8192,NULL,4,&s_task,1)!=pdPASS)return ESP_ERR_NO_MEM;
    return ESP_OK;
}
void recorder_start(void){s_recording=true;}
void recorder_stop(void){s_recording=false;close_segment();}
bool recorder_is_recording(void){return s_recording;}
