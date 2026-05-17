/*
 * DYA OLED modular status screen
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <dya/oled_status.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/hid.h>
#include <zmk/hid_indicators.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_WPM)
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>
#endif

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL)
#include <zmk/split/central.h>
#endif

#include <dt-bindings/zmk/modifiers.h>
#include <dya/oled_status_animation.h>

#define SCREEN_W CONFIG_DYA_OLED_STATUS_WIDTH
#define SCREEN_H CONFIG_DYA_OLED_STATUS_HEIGHT
#define MAX_PERIPHERALS 4

struct dya_oled_status_widget {
    sys_snode_t node;
    lv_obj_t *root;
    lv_obj_t *battery_central;
    lv_obj_t *battery_peripheral[MAX_PERIPHERALS];
    lv_obj_t *output;
    lv_obj_t *layer;
    lv_obj_t *wpm;
    lv_obj_t *modifiers;
    lv_obj_t *hid;
    lv_obj_t *animation;
    lv_timer_t *animation_timer;
    uint8_t animation_frame;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static struct dya_oled_status_widget status_widget;
static lv_style_t screen_style;
static lv_style_t label_style;

static void set_label(lv_obj_t *label, const char *text) {
    if (label != NULL) {
        lv_label_set_text(label, text);
    }
}

static void set_visible(lv_obj_t *obj, bool visible) {
    if (obj == NULL) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_coord_t width) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_add_style(label, &label_style, LV_PART_MAIN);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, text);
    return label;
}

static bool widget_visible(bool compiled, bool runtime_enabled) {
    return compiled && runtime_enabled;
}

static bool animation_visible(const struct dya_oled_status_runtime_config *config) {
    return config->animation == DYA_OLED_STATUS_ANIMATION_CYBER_FACE;
}

static void update_animation_frame(struct dya_oled_status_widget *widget) {
    if (widget == NULL || widget->animation == NULL) {
        return;
    }

    lv_img_set_src(widget->animation, dya_oled_status_cyber_face_frame(widget->animation_frame));
}

static void animation_timer_cb(lv_timer_t *timer) {
    struct dya_oled_status_widget *widget = timer->user_data;
    if (widget == NULL || widget->animation == NULL ||
        !animation_visible(dya_oled_status_runtime_get())) {
        return;
    }

    widget->animation_frame =
        (widget->animation_frame + 1) % DYA_OLED_STATUS_CYBER_FACE_FRAME_COUNT;
    update_animation_frame(widget);
}

static void apply_animation(struct dya_oled_status_widget *widget) {
    const bool visible = animation_visible(dya_oled_status_runtime_get());

    set_visible(widget->animation, visible);
    if (widget->animation_timer == NULL) {
        return;
    }

    if (visible) {
        update_animation_frame(widget);
        lv_timer_resume(widget->animation_timer);
    } else {
        lv_timer_pause(widget->animation_timer);
    }
}

static void apply_visibility(struct dya_oled_status_widget *widget) {
    const struct dya_oled_status_runtime_config *config = dya_oled_status_runtime_get();

    apply_animation(widget);

    set_visible(widget->battery_central,
                widget_visible(IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_CENTRAL),
                               config->show_central_battery));
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL)
    for (int i = 0; i < MAX_PERIPHERALS; i++) {
        set_visible(widget->battery_peripheral[i], config->show_peripheral_batteries);
    }
#endif
    set_visible(widget->output,
                widget_visible(IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_OUTPUT), config->show_output));
    set_visible(widget->layer,
                widget_visible(IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_LAYER), config->show_layer));
    set_visible(widget->wpm,
                widget_visible(IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_WPM), config->show_wpm));
    set_visible(widget->modifiers,
                widget_visible(IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_MODIFIERS), config->show_modifiers));
    set_visible(widget->hid, widget_visible(IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_HID_INDICATORS),
                                            config->show_hid_indicators));
}

static void align_if_visible(lv_obj_t *obj, lv_align_t align, lv_coord_t x, lv_coord_t y,
                             lv_coord_t *cursor, lv_coord_t row_h) {
    if (obj == NULL || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    if (cursor != NULL) {
        lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 0, *cursor);
        *cursor += row_h;
    } else {
        lv_obj_align(obj, align, x, y);
    }
}

static void apply_layout(struct dya_oled_status_widget *widget) {
    const struct dya_oled_status_runtime_config *config = dya_oled_status_runtime_get();
    const bool portrait = config->orientation == DYA_OLED_STATUS_ORIENTATION_PORTRAIT;

    apply_visibility(widget);

    if (portrait) {
        lv_coord_t y = 0;
        const lv_coord_t row_h = 12;

        if (animation_visible(config)) {
            lv_obj_align(widget->animation, LV_ALIGN_BOTTOM_MID, 0, 0);
        }
        align_if_visible(widget->output, LV_ALIGN_TOP_LEFT, 0, 0, &y, row_h);
        align_if_visible(widget->battery_central, LV_ALIGN_TOP_LEFT, 0, 0, &y, row_h);
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL)
        for (int i = 0; i < MAX_PERIPHERALS; i++) {
            align_if_visible(widget->battery_peripheral[i], LV_ALIGN_TOP_LEFT, 0, 0, &y, row_h);
        }
#endif
        align_if_visible(widget->layer, LV_ALIGN_TOP_LEFT, 0, 0, &y, row_h);
        align_if_visible(widget->wpm, LV_ALIGN_TOP_LEFT, 0, 0, &y, row_h);
        align_if_visible(widget->modifiers, LV_ALIGN_TOP_LEFT, 0, 0, &y, row_h);
        align_if_visible(widget->hid, LV_ALIGN_TOP_LEFT, 0, 0, &y, row_h);
        return;
    }

    if (animation_visible(config)) {
        lv_obj_align(widget->animation, LV_ALIGN_CENTER, 0, 0);
    }
    align_if_visible(widget->output, LV_ALIGN_TOP_LEFT, 0, 0, NULL, 0);
    align_if_visible(widget->battery_central, LV_ALIGN_TOP_RIGHT, 0, 0, NULL, 0);
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL)
    int visible_index = 0;
    for (int i = 0; i < MAX_PERIPHERALS; i++) {
        if (widget->battery_peripheral[i] == NULL ||
            lv_obj_has_flag(widget->battery_peripheral[i], LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }
        lv_obj_align(widget->battery_peripheral[i], LV_ALIGN_TOP_RIGHT, 0, 12 + (visible_index * 10));
        visible_index++;
    }
#endif
    align_if_visible(widget->layer, LV_ALIGN_BOTTOM_RIGHT, 0, 0, NULL, 0);
    align_if_visible(widget->wpm, LV_ALIGN_BOTTOM_LEFT, 0, -12, NULL, 0);
    align_if_visible(widget->modifiers, LV_ALIGN_BOTTOM_LEFT, 0, 0, NULL, 0);
    align_if_visible(widget->hid, LV_ALIGN_TOP_LEFT, 0, 14, NULL, 0);
}

void dya_oled_status_display_refresh(void) {
    struct dya_oled_status_widget *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        apply_layout(widget);
    }
}

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_CENTRAL)
struct central_battery_state {
    uint8_t level;
    bool usb_present;
};

static void update_central_battery_cb(struct central_battery_state state) {
    struct dya_oled_status_widget *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (state.usb_present) {
            lv_label_set_text_fmt(widget->battery_central, "C %u%% USB", state.level);
        } else {
            lv_label_set_text_fmt(widget->battery_central, "C %u%%", state.level);
        }
    }
}

static struct central_battery_state central_battery_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct central_battery_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#else
        .usb_present = false,
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(dya_oled_central_battery, struct central_battery_state,
                            update_central_battery_cb, central_battery_get_state)
ZMK_SUBSCRIPTION(dya_oled_central_battery, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(dya_oled_central_battery, zmk_usb_conn_state_changed);
#endif
#endif

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL)
struct peripheral_battery_state {
    uint8_t source;
    uint8_t level;
    bool valid;
};

static void update_peripheral_battery_cb(struct peripheral_battery_state state) {
    if (!state.valid || state.source >= MAX_PERIPHERALS) {
        return;
    }

    struct dya_oled_status_widget *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (widget->battery_peripheral[state.source] != NULL) {
            lv_label_set_text_fmt(widget->battery_peripheral[state.source], "P%u %u%%", state.source + 1,
                                  state.level);
        }
    }
}

static struct peripheral_battery_state peripheral_battery_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    if (ev != NULL) {
        return (struct peripheral_battery_state){
            .source = ev->source,
            .level = ev->state_of_charge,
            .valid = true,
        };
    }

    for (uint8_t i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT && i < MAX_PERIPHERALS; i++) {
        uint8_t level;
        if (zmk_split_central_get_peripheral_battery_level(i, &level) == 0) {
            return (struct peripheral_battery_state){.source = i, .level = level, .valid = true};
        }
    }

    return (struct peripheral_battery_state){.valid = false};
}

static void refresh_peripheral_battery_cache(void) {
    for (uint8_t i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT && i < MAX_PERIPHERALS; i++) {
        uint8_t level;
        if (zmk_split_central_get_peripheral_battery_level(i, &level) == 0) {
            update_peripheral_battery_cb((struct peripheral_battery_state){
                .source = i,
                .level = level,
                .valid = true,
            });
        }
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(dya_oled_peripheral_battery, struct peripheral_battery_state,
                            update_peripheral_battery_cb, peripheral_battery_get_state)
ZMK_SUBSCRIPTION(dya_oled_peripheral_battery, zmk_peripheral_battery_state_changed);
#endif

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_OUTPUT)
struct output_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
};

static void update_output_cb(struct output_state state) {
    struct dya_oled_status_widget *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (state.selected_endpoint.transport == ZMK_TRANSPORT_USB) {
            set_label(widget->output, "USB");
        } else {
            lv_label_set_text_fmt(widget->output, "BT%d %s", state.active_profile_index + 1,
                                  state.active_profile_connected ? "OK" :
                                  (state.active_profile_bonded ? "--" : "NEW"));
        }
    }
}

static struct output_state output_get_state(const zmk_event_t *eh) {
    return (struct output_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(dya_oled_output, struct output_state, update_output_cb, output_get_state)
ZMK_SUBSCRIPTION(dya_oled_output, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(dya_oled_output, zmk_ble_active_profile_changed);
#endif
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(dya_oled_output, zmk_usb_conn_state_changed);
#endif
#endif

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_LAYER)
struct layer_state {
    uint8_t index;
    const char *label;
};

static void update_layer_cb(struct layer_state state) {
    struct dya_oled_status_widget *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (state.label != NULL && strlen(state.label) > 0) {
            lv_label_set_text_fmt(widget->layer, "L %s", state.label);
        } else {
            lv_label_set_text_fmt(widget->layer, "L %u", state.index);
        }
    }
}

static struct layer_state layer_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(dya_oled_layer, struct layer_state, update_layer_cb, layer_get_state)
ZMK_SUBSCRIPTION(dya_oled_layer, zmk_layer_state_changed);
#endif

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_WPM)
struct wpm_state {
    uint8_t wpm;
};

static void update_wpm_cb(struct wpm_state state) {
    struct dya_oled_status_widget *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        lv_label_set_text_fmt(widget->wpm, "WPM %u", state.wpm);
    }
}

static struct wpm_state wpm_get_state(const zmk_event_t *eh) {
    return (struct wpm_state){.wpm = zmk_wpm_get_state()};
}

ZMK_DISPLAY_WIDGET_LISTENER(dya_oled_wpm, struct wpm_state, update_wpm_cb, wpm_get_state)
ZMK_SUBSCRIPTION(dya_oled_wpm, zmk_wpm_state_changed);
#endif

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_MODIFIERS)
struct modifiers_state {
    zmk_mod_flags_t mods;
};

static void update_modifiers_cb(struct modifiers_state state) {
    char text[20] = "M ";
    bool any = false;

    if (state.mods & (MOD_LCTL | MOD_RCTL)) {
        strcat(text, "C");
        any = true;
    }
    if (state.mods & (MOD_LSFT | MOD_RSFT)) {
        strcat(text, "S");
        any = true;
    }
    if (state.mods & (MOD_LALT | MOD_RALT)) {
        strcat(text, "A");
        any = true;
    }
    if (state.mods & (MOD_LGUI | MOD_RGUI)) {
        strcat(text, "G");
        any = true;
    }
    if (!any) {
        strcat(text, "-");
    }

    struct dya_oled_status_widget *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_label(widget->modifiers, text); }
}

static struct modifiers_state modifiers_get_state(const zmk_event_t *eh) {
    return (struct modifiers_state){.mods = zmk_hid_get_explicit_mods()};
}

ZMK_DISPLAY_WIDGET_LISTENER(dya_oled_modifiers, struct modifiers_state, update_modifiers_cb,
                            modifiers_get_state)
ZMK_SUBSCRIPTION(dya_oled_modifiers, zmk_keycode_state_changed);
#endif

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_HID_INDICATORS)
struct hid_state {
    zmk_hid_indicators_t indicators;
};

static void update_hid_cb(struct hid_state state) {
    char text[20] = "H ";
    bool any = false;

    if (state.indicators & BIT(1)) {
        strcat(text, "CAP ");
        any = true;
    }
    if (state.indicators & BIT(0)) {
        strcat(text, "NUM ");
        any = true;
    }
    if (state.indicators & BIT(2)) {
        strcat(text, "SCR");
        any = true;
    }
    if (!any) {
        strcat(text, "-");
    }

    struct dya_oled_status_widget *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_label(widget->hid, text); }
}

static struct hid_state hid_get_state(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    return (struct hid_state){
        .indicators = (ev != NULL) ? ev->indicators : zmk_hid_indicators_get_current_profile(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(dya_oled_hid, struct hid_state, update_hid_cb, hid_get_state)
ZMK_SUBSCRIPTION(dya_oled_hid, zmk_hid_indicators_changed);
#endif

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_size(screen, SCREEN_W, SCREEN_H);

    lv_style_init(&screen_style);
    lv_style_set_bg_color(&screen_style, lv_color_black());
    lv_style_set_bg_opa(&screen_style, LV_OPA_COVER);
    lv_style_set_border_width(&screen_style, 0);
    lv_style_set_pad_all(&screen_style, 0);
    lv_obj_add_style(screen, &screen_style, LV_PART_MAIN);

    lv_style_init(&label_style);
    lv_style_set_text_color(&label_style, lv_color_white());
    lv_style_set_text_font(&label_style, &lv_font_unscii_8);
    lv_style_set_pad_all(&label_style, 0);
    lv_style_set_bg_opa(&label_style, LV_OPA_TRANSP);

    status_widget.root = screen;
    status_widget.animation = lv_img_create(screen);
    status_widget.animation_frame = 0;
    lv_img_set_src(status_widget.animation, dya_oled_status_cyber_face_frame(0));
    status_widget.animation_timer =
        lv_timer_create(animation_timer_cb, DYA_OLED_STATUS_CYBER_FACE_FRAME_MS, &status_widget);
    lv_timer_pause(status_widget.animation_timer);

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_CENTRAL)
    status_widget.battery_central = make_label(screen, "C --%", 56);
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL)
    for (int i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT && i < MAX_PERIPHERALS; i++) {
        status_widget.battery_peripheral[i] = make_label(screen, "P- --%", 56);
    }
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_OUTPUT)
    status_widget.output = make_label(screen, "OUT", 42);
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_LAYER)
    status_widget.layer = make_label(screen, "L -", CONFIG_DYA_OLED_STATUS_LAYER_WIDTH);
    lv_label_set_long_mode(status_widget.layer, LV_LABEL_LONG_SCROLL_CIRCULAR);
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_WPM)
    status_widget.wpm = make_label(screen, "WPM 0", 42);
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_MODIFIERS)
    status_widget.modifiers = make_label(screen, "M -", 42);
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_HID_INDICATORS)
    status_widget.hid = make_label(screen, "H -", 64);
#endif

    sys_slist_append(&widgets, &status_widget.node);

#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_CENTRAL)
    dya_oled_central_battery_init();
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL)
    dya_oled_peripheral_battery_init();
    refresh_peripheral_battery_cache();
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_OUTPUT)
    dya_oled_output_init();
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_LAYER)
    dya_oled_layer_init();
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_WPM)
    dya_oled_wpm_init();
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_MODIFIERS)
    dya_oled_modifiers_init();
#endif
#if IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_HID_INDICATORS)
    dya_oled_hid_init();
#endif

    apply_layout(&status_widget);
    return screen;
}
