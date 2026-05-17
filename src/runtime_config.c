#include <errno.h>

#include <lvgl.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#if IS_ENABLED(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif

#include <dya/oled_status.h>

#define DYA_OLED_STATUS_SETTINGS_KEY "dya_oled_status/config"

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
static enum dya_oled_status_orientation boot_orientation;
static bool runtime_config_initialized;

static void ensure_initialized(void) {
    if (!runtime_config_initialized) {
        runtime_config = default_config;
        boot_orientation = runtime_config.orientation;
        runtime_config_initialized = true;
    }
}

const struct dya_oled_status_runtime_config *dya_oled_status_runtime_get(void) {
    ensure_initialized();
    return &runtime_config;
}

enum dya_oled_status_orientation dya_oled_status_display_orientation_get(void) {
    ensure_initialized();
    return boot_orientation;
}

void dya_oled_status_runtime_set(const struct dya_oled_status_runtime_config *config) {
    ensure_initialized();
    if (config == NULL) {
        return;
    }
    runtime_config = *config;
#if IS_ENABLED(CONFIG_SETTINGS)
    settings_save_one(DYA_OLED_STATUS_SETTINGS_KEY, &runtime_config, sizeof(runtime_config));
#endif
}

void dya_oled_status_runtime_reset(void) {
    runtime_config = default_config;
    boot_orientation = runtime_config.orientation;
#if IS_ENABLED(CONFIG_SETTINGS)
    settings_save_one(DYA_OLED_STATUS_SETTINGS_KEY, &runtime_config, sizeof(runtime_config));
#endif
    runtime_config_initialized = true;
}

__attribute__((weak)) void dya_oled_status_display_refresh(void) {}

#if IS_ENABLED(CONFIG_SETTINGS)
static int dya_oled_status_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                        void *cb_arg) {
    const char *next;
    if (!settings_name_steq(name, "config", &next) || next != NULL) {
        return -ENOENT;
    }

    if (len != sizeof(runtime_config)) {
        return -EINVAL;
    }

    ensure_initialized();
    int rc = read_cb(cb_arg, &runtime_config, sizeof(runtime_config));
    if (rc <= 0) {
        return rc;
    }

    if (runtime_config.orientation != DYA_OLED_STATUS_ORIENTATION_PORTRAIT) {
        runtime_config.orientation = DYA_OLED_STATUS_ORIENTATION_LANDSCAPE;
    }
    if (runtime_config.animation != DYA_OLED_STATUS_ANIMATION_CYBER_FACE) {
        runtime_config.animation = DYA_OLED_STATUS_ANIMATION_OFF;
    }

    boot_orientation = runtime_config.orientation;
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(dya_oled_status, "dya_oled_status", NULL,
                               dya_oled_status_settings_set, NULL, NULL);

static int dya_oled_status_preload_settings(void) {
    int rc = settings_subsys_init();
    if (rc != 0) {
        return rc;
    }

    return settings_load_subtree("dya_oled_status");
}

/* ZMK loads settings in main(), which is too late for LVGL display setup. */
SYS_INIT(dya_oled_status_preload_settings, APPLICATION,
         UTIL_DEC(CONFIG_APPLICATION_INIT_PRIORITY));
#endif

static int dya_oled_status_apply_boot_display_rotation(void) {
    ensure_initialized();

    if (boot_orientation != DYA_OLED_STATUS_ORIENTATION_PORTRAIT) {
        return 0;
    }

    lv_disp_t *disp = lv_disp_get_default();
    if (disp == NULL || disp->driver == NULL) {
        return -ENODEV;
    }

    /* SSD1306 does not implement display_set_orientation(); use LVGL software rotation. */
    disp->driver->sw_rotate = 1;
    lv_disp_set_rotation(disp, LV_DISP_ROT_90);
    return 0;
}

SYS_INIT(dya_oled_status_apply_boot_display_rotation, APPLICATION,
         UTIL_INC(CONFIG_APPLICATION_INIT_PRIORITY));
