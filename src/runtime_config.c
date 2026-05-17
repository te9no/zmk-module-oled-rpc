#include <zephyr/sys/util.h>
#include <dya/oled_status.h>

static const struct dya_oled_status_runtime_config default_config = {
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_PORTRAIT)
    .orientation = DYA_OLED_STATUS_ORIENTATION_PORTRAIT,
#else
    .orientation = DYA_OLED_STATUS_ORIENTATION_LANDSCAPE,
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_ANIMATION_CYBER_FACE)
    .animation = DYA_OLED_STATUS_ANIMATION_CYBER_FACE,
#else
    .animation = DYA_OLED_STATUS_ANIMATION_OFF,
#endif
    .show_central_battery = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_CENTRAL),
    .show_peripheral_batteries = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL),
    .show_output = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_OUTPUT),
    .show_layer = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_LAYER),
    .show_wpm = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_WPM),
    .show_modifiers = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_MODIFIERS),
    .show_hid_indicators = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_HID_INDICATORS),
};

static struct dya_oled_status_runtime_config runtime_config;
static bool runtime_config_initialized;

static void ensure_initialized(void) {
    if (!runtime_config_initialized) {
        runtime_config = default_config;
        runtime_config_initialized = true;
    }
}

const struct dya_oled_status_runtime_config *dya_oled_status_runtime_get(void) {
    ensure_initialized();
    return &runtime_config;
}

void dya_oled_status_runtime_set(const struct dya_oled_status_runtime_config *config) {
    ensure_initialized();
    if (config == NULL) {
        return;
    }
    runtime_config = *config;
}

void dya_oled_status_runtime_reset(void) {
    runtime_config = default_config;
    runtime_config_initialized = true;
}

__attribute__((weak)) void dya_oled_status_display_refresh(void) {}
