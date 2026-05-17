#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum dya_oled_status_orientation {
    DYA_OLED_STATUS_ORIENTATION_LANDSCAPE = 0,
    DYA_OLED_STATUS_ORIENTATION_PORTRAIT = 1,
};

enum dya_oled_status_animation {
    DYA_OLED_STATUS_ANIMATION_OFF = 0,
    DYA_OLED_STATUS_ANIMATION_CYBER_FACE = 1,
};

struct dya_oled_status_runtime_config {
    enum dya_oled_status_orientation orientation;
    enum dya_oled_status_animation animation;
    bool show_central_battery;
    bool show_peripheral_batteries;
    bool show_output;
    bool show_layer;
    bool show_wpm;
    bool show_modifiers;
    bool show_hid_indicators;
};

const struct dya_oled_status_runtime_config *dya_oled_status_runtime_get(void);
enum dya_oled_status_orientation dya_oled_status_display_orientation_get(void);
void dya_oled_status_runtime_set(const struct dya_oled_status_runtime_config *config);
void dya_oled_status_runtime_reset(void);
void dya_oled_status_display_refresh(void);

#ifdef __cplusplus
}
#endif
