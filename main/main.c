#include <stdio.h>
#include "esp_log.h"
#include "esp_camera.h"
#include "sdmmc_cmd.h" 
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/sys.h"
#include "lwip/err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

// Enter actual Wi-Fi details
#define ESP_WIFI_SSID "YourSSIDHere"
#define ESP_WIFI_PASS "12345678"
#define ESP_MAXIMUM_RETRY 2

static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "MAIN_APP";

// we are using ESP32-S3-WROOM-1-N8R8 with 8MiB Flash Storage and 8MiB PSRAM
// update pin numbers later

#define CAM_PIN_D0 1
#define CAM_PIN_D1 1
#define CAM_PIN_D2 1
#define CAM_PIN_D3 1
#define CAM_PIN_D4 1
#define CAM_PIN_D5 1
#define CAM_PIN_D6 1
#define CAM_PIN_D7 1

#define CAM_PIN_VSYNC 1
#define CAM_PIN_HREF 1

#define CAM_PIN_SIOD 1
#define CAM_PIN_SIOC 1

#define CAM_PIN_XCLK 1
#define CAM_PIN_PCLK 1

// these two pins aren't used, thus initialised as -1:
#define CAM_PIN_RST -1
#define CAM_PIN_PWDN -1


// SD card pin connections: (to be updated)
#define SD_D0 1
#define SD_D1 1
#define SD_D2 1
#define SD_D3 1
#define SD_CMD 1
#define SD_CLK 1

const camera_config_t cam_conf = {
    .pin_d0 = CAM_PIN_D0,
    .pin_d1 = CAM_PIN_D1,
    .pin_d2 = CAM_PIN_D2,
    .pin_d3 = CAM_PIN_D3,
    .pin_d4 = CAM_PIN_D4,
    .pin_d5 = CAM_PIN_D5,
    .pin_d6 = CAM_PIN_D6,
    .pin_d7 = CAM_PIN_D7,

    .pin_xclk = CAM_PIN_XCLK,
    .pin_pclk = CAM_PIN_PCLK,

    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,

    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_sccb_sda = CAM_PIN_SIOD,

    .pin_reset = CAM_PIN_RST,
    .pin_pwdn = CAM_PIN_PWDN,

    .xclk_freq_hz = 20000000, // 20MHz xclk freq

    .ledc_channel = LEDC_CHANNEL_0,
    .ledc_timer = LEDC_TIMER_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_UXGA,

    .jpeg_quality = 16,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
    // .jpeg_buffer_size = 2
};

esp_err_t espcam_init(const camera_config_t *config){
    esp_err_t err = esp_camera_init(config);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Camera issue");
        return err;
    }
    return ESP_OK;
}

esp_err_t sd_card_init(){
    sdmmc_card_t sdcard;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_conf = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_conf.width = 4;
    slot_conf.clk = SD_CLK;
    slot_conf.cmd = SD_CMD;
    slot_conf.d0 = SD_D0;
    slot_conf.d1 = SD_D1;
    slot_conf.d2 = SD_D2;
    slot_conf.d3 = SD_D3;

    esp_vfs_fat_mount_config_t mount_conf = {
        .max_files = 10,
        .allocation_unit_size = 16*1024,
        .format_if_mount_failed = false,
        .disk_status_check_enable = false,
        .use_one_fat = false
    };
    // esp_vfs_fat_conf_t fat_conf = {
    //     .base_path = "/sdcard",
    //     .fat_drive = "C",
    //     .max_files = 10
    // };
    esp_err_t mount = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_conf, &mount_conf, &sdcard);
    if(mount != ESP_OK){
        return mount;
    }

    return ESP_OK;
}
// event handler code directly taken from the ESP-IDF library
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// wifi initialisation example taken from the esp-idf example because i am relatively new to esp-32
void wifi_init_sta(){
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t conf = WIFI_INIT_CONFIG_DEFAULT();
    EWSP_ERROR_CHECK(esp_wifi_init(&conf));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    wifi_config_t wifi_conf = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_MODE_STA, &wifi_conf));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    espcam_init(&cam_conf);
    sd_card_init();


    esp_err_t nvsinit = nvs_flash_init();
    if(nvsinit != ESP_OK){
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvsinit = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvsinit);
    wifi_init_sta();
}