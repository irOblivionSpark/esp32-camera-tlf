#include "config.h"
#include "camera_pinout.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG="cam";
static volatile bool recording=RECORD_ON_BOOT;
static FILE *rf=NULL;
static int64_t segment_start=0;
static unsigned seq=0;
static bool wifi_ok=false;

static camera_config_t cam_cfg={
 .pin_pwdn=CAM_PIN_PWDN,.pin_reset=CAM_PIN_RESET,.pin_xclk=CAM_PIN_XCLK,
 .pin_sccb_sda=CAM_PIN_SIOD,.pin_sccb_scl=CAM_PIN_SIOC,
 .pin_d7=CAM_PIN_D7,.pin_d6=CAM_PIN_D6,.pin_d5=CAM_PIN_D5,.pin_d4=CAM_PIN_D4,
 .pin_d3=CAM_PIN_D3,.pin_d2=CAM_PIN_D2,.pin_d1=CAM_PIN_D1,.pin_d0=CAM_PIN_D0,
 .pin_vsync=CAM_PIN_VSYNC,.pin_href=CAM_PIN_HREF,.pin_pclk=CAM_PIN_PCLK,
 .xclk_freq_hz=20000000,.ledc_timer=LEDC_TIMER_0,.ledc_channel=LEDC_CHANNEL_0,
 .pixel_format=PIXFORMAT_JPEG,.frame_size=CAMERA_FRAME_SIZE,.jpeg_quality=CAMERA_JPEG_QUALITY,
 .fb_count=2,.fb_location=CAMERA_FB_IN_PSRAM,.grab_mode=CAMERA_GRAB_LATEST
};

static void wifi_evt(void *a,esp_event_base_t b,int32_t id,void *d){
 if(b==WIFI_EVENT&&id==WIFI_EVENT_STA_START) esp_wifi_connect();
 if(b==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){wifi_ok=false;vTaskDelay(pdMS_TO_TICKS(5000));esp_wifi_connect();}
 if(b==IP_EVENT&&id==IP_EVENT_STA_GOT_IP) wifi_ok=true;
}
static void wifi_init(void){
 ESP_ERROR_CHECK(esp_netif_init());
 esp_err_t e=esp_event_loop_create_default(); if(e!=ESP_OK&&e!=ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(e);
 esp_netif_t*n=esp_netif_create_default_wifi_sta();
 esp_netif_ip_info_t ip={0};
 ip4addr_aton(STATIC_IP,&ip.ip);ip4addr_aton(GATEWAY,&ip.gw);ip4addr_aton(SUBNET_MASK,&ip.netmask);
 ESP_ERROR_CHECK(esp_netif_dhcpc_stop(n));ESP_ERROR_CHECK(esp_netif_set_ip_info(n,&ip));
 esp_netif_dns_info_t dns={0};dns.ip.type=ESP_IPADDR_TYPE_V4;
 ip4addr_aton(DNS1,&dns.ip.u_addr.ip4);ESP_ERROR_CHECK(esp_netif_set_dns_info(n,ESP_NETIF_DNS_MAIN,&dns));
 wifi_init_config_t c=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&c));
 ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_evt,NULL));
 ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_evt,NULL));
 wifi_config_t w={0};strlcpy((char*)w.sta.ssid,WIFI_SSID,sizeof(w.sta.ssid));strlcpy((char*)w.sta.password,WIFI_PASSWORD,sizeof(w.sta.password));
 ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&w));ESP_ERROR_CHECK(esp_wifi_start());
}
static void sd_init(void){
 esp_vfs_fat_sdmmc_mount_config_t m={.format_if_mount_failed=false,.max_files=8,.allocation_unit_size=16*1024};
 sdmmc_host_t h=SDMMC_HOST_DEFAULT();sdmmc_slot_config_t s=SDMMC_SLOT_CONFIG_DEFAULT();s.width=1;
 sdmmc_card_t *card=NULL;
 ESP_ERROR_CHECK(esp_vfs_fat_sdmmc_mount("/sdcard",&h,&s,&m,&card));
 mkdir("/sdcard/recordings",0775);
}
static void close_seg(void){if(rf){fflush(rf);fclose(rf);rf=NULL;}}
static bool open_seg(void){
 time_t now;struct tm t;time(&now);localtime_r(&now,&t);char p[180];
 snprintf(p,sizeof(p),"/sdcard/recordings/cam_%04d%02d%02d_%02d%02d%02d_%u.mjpg",t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec,seq++);
 rf=fopen(p,"wb");segment_start=esp_timer_get_time()/1000;return rf!=NULL;
}
static void recorder(void*a){
 for(;;){
  if(!recording){vTaskDelay(pdMS_TO_TICKS(100));continue;}
  if(!rf&&!open_seg()){recording=false;continue;}
  if(esp_timer_get_time()/1000-segment_start>SEGMENT_SECONDS*1000LL){close_seg();continue;}
  camera_fb_t*f=esp_camera_fb_get();
  if(f){if(rf&&f->format==PIXFORMAT_JPEG)fwrite(f->buf,1,f->len,rf);esp_camera_fb_return(f);}
  vTaskDelay(pdMS_TO_TICKS(1000/RECORD_FPS));
 }
}
static const char *html="<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>ESP32-CAM</title><style>body{font-family:system-ui;background:#101218;color:#eee;max-width:900px;margin:auto;padding:20px}img{width:100%;border-radius:12px}button{padding:12px 18px;margin:5px;border:0;border-radius:8px}a{color:#7db7ff}</style><h1>ESP32-CAM</h1><img src=/stream><p id=s>...</p><button onclick=\"go('/api/start')\">START</button><button onclick=\"go('/api/stop')\">STOP</button><pre id=i></pre><h2>Recordings</h2><div id=f></div><script>async function go(x){await fetch(x,{method:'POST'});load()}async function load(){let s=await(await fetch('/api/status')).json();document.querySelector('#s').textContent=s.rec?'RECORDING':'STOPPED';document.querySelector('#i').textContent='WiFi: '+s.wifi+'\\nIP: '+s.ip+'\\nRSSI: '+s.rssi+' dBm';let f=await(await fetch('/api/files')).json();document.querySelector('#f').innerHTML=f.map(x=>'<p><a href=\"/api/file?name='+encodeURIComponent(x)+'\">'+x+'</a></p>').join('')}setInterval(load,3000);load()</script>";

static esp_err_t root(httpd_req_t*r){httpd_resp_set_type(r,"text/html");return httpd_resp_send(r,html,HTTPD_RESP_USE_STRLEN);}
static esp_err_t stream(httpd_req_t*r){
 httpd_resp_set_type(r,"multipart/x-mixed-replace;boundary=frame");
 for(;;){camera_fb_t*f=esp_camera_fb_get();if(!f)break;
  char h[80];if(httpd_resp_send_chunk(r,"\\r\\n--frame\\r\\n",13)!=ESP_OK){esp_camera_fb_return(f);break;}
  int n=snprintf(h,sizeof(h),"Content-Type: image/jpeg\\r\\nContent-Length: %u\\r\\n\\r\\n",(unsigned)f->len);
  esp_err_t e=httpd_resp_send_chunk(r,h,n);if(e==ESP_OK)e=httpd_resp_send_chunk(r,(char*)f->buf,f->len);
  esp_camera_fb_return(f);if(e!=ESP_OK)break;vTaskDelay(pdMS_TO_TICKS(125));
 }return ESP_OK;
}
static esp_err_t status(httpd_req_t*r){wifi_ap_record_t a={0};int rss=0;if(wifi_ok&&esp_wifi_sta_get_ap_info(&a)==ESP_OK)rss=a.rssi;char j[240];snprintf(j,sizeof(j),"{\"rec\":%s,\"wifi\":\"%s\",\"ip\":\"%s\",\"rssi\":%d}",recording?"true":"false",WIFI_SSID,STATIC_IP,rss);httpd_resp_set_type(r,"application/json");return httpd_resp_send(r,j,HTTPD_RESP_USE_STRLEN);}
static esp_err_t start(httpd_req_t*r){recording=true;return httpd_resp_sendstr(r,"OK");}
static esp_err_t stop(httpd_req_t*r){recording=false;close_seg();return httpd_resp_sendstr(r,"OK");}
static esp_err_t files(httpd_req_t*r){DIR*d=opendir("/sdcard/recordings");if(!d)return httpd_resp_send_500(r);httpd_resp_set_type(r,"application/json");httpd_resp_sendstr_chunk(r,"[");struct dirent*e;bool first=true;while((e=readdir(d))){if(strstr(e->d_name,".mjpg")){char x[220];snprintf(x,sizeof(x),"%s\"%s\"",first?"":",",e->d_name);httpd_resp_sendstr_chunk(r,x);first=false;}}closedir(d);return httpd_resp_sendstr_chunk(r,"]");}
static esp_err_t file(httpd_req_t*r){char q[200],n[150];if(httpd_req_get_url_query_str(r,q,sizeof(q))!=ESP_OK||httpd_query_key_value(q,"name",n,sizeof(n))!=ESP_OK)return httpd_resp_send_400(r,NULL);if(strstr(n,"..")||strchr(n,'/')||strchr(n,'\\'))return httpd_resp_send_err(r,403,"bad");char p[220];snprintf(p,sizeof(p),"/sdcard/recordings/%s",n);FILE*f=fopen(p,"rb");if(!f)return httpd_resp_send_404(r);httpd_resp_set_type(r,"application/octet-stream");char b[4096];size_t z;while((z=fread(b,1,sizeof(b),f))){if(httpd_resp_send_chunk(r,b,z)!=ESP_OK)break;}fclose(f);return httpd_resp_send_chunk(r,NULL,0);}
static void web(void){httpd_config_t c=HTTPD_DEFAULT_CONFIG();c.server_port=WEB_PORT;httpd_handle_t h;ESP_ERROR_CHECK(httpd_start(&h,&c));httpd_uri_t u[]={{"/",HTTP_GET,root,NULL},{"/stream",HTTP_GET,stream,NULL},{"/api/status",HTTP_GET,status,NULL},{"/api/start",HTTP_POST,start,NULL},{"/api/stop",HTTP_POST,stop,NULL},{"/api/files",HTTP_GET,files,NULL},{"/api/file",HTTP_GET,file,NULL}};for(int i=0;i<7;i++)httpd_register_uri_handler(h,&u[i]);}
void app_main(void){
 esp_err_t n=nvs_flash_init();if(n==ESP_ERR_NVS_NO_FREE_PAGES||n==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());ESP_ERROR_CHECK(nvs_flash_init());}
 ESP_ERROR_CHECK(esp_camera_init(&cam_cfg));sd_init();wifi_init();web();xTaskCreatePinnedToCore(recorder,"rec",8192,NULL,4,NULL,1);ESP_LOGI(TAG,"Web: http://%s",STATIC_IP);
}
