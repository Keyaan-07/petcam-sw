#include <stdio.h>
#include "esp_camera.h"


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

static camera_config_t cam_conf = {
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
};

void app_main(void)
{
    
}
