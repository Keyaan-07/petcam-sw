#include <stdio.h>
#include "esp_log.h"
#include "esp_camera.h"
#include "sdmmc_cmd.h" 
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"

// we are using ESP32-S3-WROOM-1-N8R8 with 8MiB Flash Storage and 8MiB PSRAM
// update pin numbers later

static const char *TAG = "MAIN_APP";

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

void app_main(void)
{
    espcam_init(&cam_conf);
    sd_card_init();
}
