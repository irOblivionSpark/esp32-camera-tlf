#include "wifi_manager.h"
#include "config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "nvs_flash.h"

static const char *TAG="wifi";
static esp_netif_t *s_netif;
static volatile bool s_connected=false;

static void wifi_event_handler(void *arg,esp_event_base_t base,int32_t id,void *data){
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_START) esp_wifi_connect();
    else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED){
        s_connected=false; ESP_LOGW(TAG,"WiFi disconnected; reconnecting");
        vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS)); esp_wifi_connect();
    } else if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *e=(ip_event_got_ip_t*)data; s_connected=true;
        ESP_LOGI(TAG,"IP: " IPSTR,IP2STR(&e->ip_info.ip));
    }
}
esp_err_t wifi_manager_init(void){
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t e=esp_event_loop_create_default();
    if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE) return e;
    s_netif=esp_netif_create_default_wifi_sta(); if(!s_netif) return ESP_FAIL;
    esp_netif_ip_info_t ip={0};
    ip4addr_aton(STATIC_IP,&ip.ip); ip4addr_aton(GATEWAY,&ip.gw); ip4addr_aton(SUBNET_MASK,&ip.netmask);
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(s_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif,&ip));
    esp_netif_dns_info_t dns={0};
    dns.ip.type=ESP_IPADDR_TYPE_V4; ip4addr_aton(PRIMARY_DNS,&dns.ip.u_addr.ip4);
    ESP_ERROR_CHECK(esp_netif_set_dns_info(s_netif,ESP_NETIF_DNS_MAIN,&dns));
    ip4addr_aton(SECONDARY_DNS,&dns.ip.u_addr.ip4);
    ESP_ERROR_CHECK(esp_netif_set_dns_info(s_netif,ESP_NETIF_DNS_BACKUP,&dns));
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifi_event_handler,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&wifi_event_handler,NULL));
    wifi_config_t wc={0};
    strlcpy((char*)wc.sta.ssid,WIFI_SSID,sizeof(wc.sta.ssid));
    strlcpy((char*)wc.sta.password,WIFI_PASSWORD,sizeof(wc.sta.password));
    wc.sta.pmf_cfg.capable=true; wc.sta.pmf_cfg.required=false;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}
bool wifi_manager_is_connected(void){return s_connected;}
