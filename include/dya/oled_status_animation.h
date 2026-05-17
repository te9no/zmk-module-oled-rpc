#pragma once

#include <stdint.h>
#include <lvgl.h>

#define DYA_OLED_STATUS_CYBER_FACE_FRAME_COUNT 4
#define DYA_OLED_STATUS_CYBER_FACE_FRAME_MS 250

const lv_img_dsc_t *dya_oled_status_cyber_face_frame(uint8_t index);
