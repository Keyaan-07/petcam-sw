#include <stdio.h>
#include "esp_log.h"
#include "esp_camera.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "freertos/event_groups.h"
#include "nvs.h"

// Enter actual Wi-Fi details
#define ESP_WIFI_SSID "YourSSIDHere"
#define ESP_WIFI_PASS "12345678"
#define ESP_MAXIMUM_RETRY 2

static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "MAIN_APP";
bool isrecording = 0;
int32_t framenum = 0;
int32_t videonum = 0;
int needs_tobe_saved = 0;


// we are using ESP32-S3-WROOM-1-N8R8 with 8MiB Flash Storage and 8MiB PSRAM
// update pin numbers later

#define GPIO_TRIG_BTN 1
#define GPIO_LED_PIN 1

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
    
    sdmmc_card_t *sdcard;
    esp_err_t mount = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_conf, &mount_conf, &sdcard);
    if(mount != ESP_OK){
        return mount;
    }

    return ESP_OK;
}
// event handler code directly taken from the ESP-IDF library.
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
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t conf = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&conf));
    
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

esp_err_t start_cam_feed(httpd_req_t *req){
    esp_err_t errvar = ESP_OK;
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123reqendshere4567890");
    
    while(1){
        camera_fb_t *fb = esp_camera_fb_get();
        if(fb == NULL){
            ESP_LOGE(TAG, "Error, cannot get frame from the cam");
            errvar = ESP_FAIL;
            break;
        }

        char headerstr[128];
        size_t headerlen = snprintf(headerstr, sizeof(headerstr), "Content-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", fb->len);
        errvar = httpd_resp_send_chunk(req, headerstr, headerlen);
        if(errvar != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        errvar = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if (errvar != ESP_OK){
            esp_camera_fb_return(fb);
            break;
        }
        if(isrecording){
            char filename[32];
            snprintf(filename, 32, "/sdcard/petimg_%ld.jpeg", framenum++);
            FILE *openfile = fopen(filename, "w");
            fwrite(fb->buf, 1, fb->len, openfile);
            fclose(openfile);
        }

        const char *boundarystr = "\r\n--123reqendshere4567890\r\n";
        errvar = httpd_resp_send_chunk(req, boundarystr, strlen(boundarystr));
        if(errvar != ESP_OK){
            esp_camera_fb_return(fb);
            break;
        }
        
        esp_camera_fb_return(fb);
    }
    
    return errvar;
}

esp_err_t rootpage(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");

    extern const uint8_t index_html_start[] asm("_binary_index_html_start");
    extern const uint8_t index_html_end[] asm("_binary_index_html_end");
    const size_t html_size = index_html_end-index_html_start;
    return httpd_resp_send(req, (const char *)index_html_start, html_size);

}

static void IRAM_ATTR btn_handler(void* arg){
    isrecording = !isrecording;
    gpio_set_level(GPIO_LED_PIN, isrecording);
    if(!isrecording) needs_tobe_saved = true;
}

esp_err_t web_record_toggle(httpd_req_t *req){
    isrecording = !isrecording;
    gpio_set_level(GPIO_LED_PIN, isrecording);
    if(!isrecording) needs_tobe_saved = true;
    return httpd_resp_sendstr(req, "OK");
}

esp_err_t send_status(httpd_req_t *req){
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, isrecording?"Recording":"Not Recording");
}

void app_main(void)
{
    gpio_config_t btn = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .pin_bit_mask = (1ULL << GPIO_TRIG_BTN)
    };
    gpio_config(&btn);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_TRIG_BTN, btn_handler, NULL);
    gpio_reset_pin(GPIO_LED_PIN);
    gpio_set_direction(GPIO_LED_PIN, GPIO_MODE_OUTPUT);

    espcam_init(&cam_conf);
    sd_card_init();

    esp_err_t nvsinit = nvs_flash_init();
    if(nvsinit != ESP_OK){
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvsinit = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvsinit);
    wifi_init_sta(); 
    nvs_handle_t nvshand;
    nvs_open(NVS_DEFAULT_PART_NAME, NVS_READWRITE, &nvshand);
    nvs_get_i32(nvshand, "framecount", &framenum);
    nvs_get_i32(nvshand, "videocount", &videonum);
    nvs_close(nvshand);

    
    httpd_handle_t handle = NULL;
    httpd_config_t http_conf = HTTPD_DEFAULT_CONFIG();
    esp_err_t httperr = httpd_start(&handle,  &http_conf);
    if(httperr == ESP_OK){
        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = start_cam_feed,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(handle, &stream_uri);
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = rootpage,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(handle, &root_uri);
        httpd_uri_t api_record_uri = {
            .uri = "/api/record",
            .method = HTTP_GET,
            .handler = web_record_toggle,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(handle, &api_record_uri);
        httpd_uri_t api_status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = send_status,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(handle, &api_status_uri);
        ESP_LOGI(TAG, "Web server is running, along with the root site and the record API");
    }    
    while(1){
        if(needs_tobe_saved == 1 && isrecording == 0){
            char vidfilename[32];
            snprintf(vidfilename, 32, "/sdcard/recording_%ld.mjpeg", videonum);
            FILE *videofile = fopen(vidfilename, "w");
            if(videofile != NULL){
                for(int32_t i = 0; i< framenum ; i++){
                    char imagefilename[32];
                    snprintf(imagefilename, 32, "/sdcard/petimg_%ld.jpeg", i);
                    FILE *imagefile = fopen(imagefilename, "r");

                    if(imagefile != NULL){
                        uint8_t buffer[1024];
                        size_t bytes_read = 0;

                        while ((bytes_read = fread(buffer, 1, 1024, imagefile)) > 0 ){
                            fwrite(buffer, 1, bytes_read, videofile);
                        }

                        fclose(imagefile);
                        unlink(imagefilename);

                    }

                }
                fclose(videofile);
                framenum = 0;
                videonum++;
            }


            nvs_handle_t nvshand;
            nvs_open(NVS_DEFAULT_PART_NAME, NVS_READWRITE, &nvshand);
            nvs_set_i32(nvshand, "framecount", framenum);
            nvs_commit(nvshand);
            nvs_set_i32(nvshand, "videocount", videonum);
            nvs_commit(nvshand);
            nvs_close(nvshand);
            needs_tobe_saved = 0;
        }
        vTaskDelay(100);
    }
}