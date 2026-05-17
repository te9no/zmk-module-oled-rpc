#include <pb_decode.h>
#include <pb_encode.h>
#include <stdio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <dya/oled_status.h>
#include <dya/oled_status/oled_status.pb.h>
#include <zmk/studio/custom.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_rpc_custom_subsystem_meta dya_oled_status_meta = {
    ZMK_RPC_CUSTOM_SUBSYSTEM_UI_URLS("https://yourname.github.io/zmk-module-oled-rpc/"),
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

ZMK_RPC_CUSTOM_SUBSYSTEM(dya__oled_status, &dya_oled_status_meta,
                         dya_oled_status_rpc_handle_request);
ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(dya__oled_status, dya_oled_status_Response);

static dya_oled_status_Orientation proto_orientation(enum dya_oled_status_orientation orientation) {
    return orientation == DYA_OLED_STATUS_ORIENTATION_PORTRAIT
               ? dya_oled_status_Orientation_ORIENTATION_PORTRAIT
               : dya_oled_status_Orientation_ORIENTATION_LANDSCAPE;
}

static enum dya_oled_status_orientation runtime_orientation(dya_oled_status_Orientation orientation) {
    return orientation == dya_oled_status_Orientation_ORIENTATION_PORTRAIT
               ? DYA_OLED_STATUS_ORIENTATION_PORTRAIT
               : DYA_OLED_STATUS_ORIENTATION_LANDSCAPE;
}

static dya_oled_status_Animation proto_animation(enum dya_oled_status_animation animation) {
    return animation == DYA_OLED_STATUS_ANIMATION_CYBER_FACE
               ? dya_oled_status_Animation_ANIMATION_CYBER_FACE
               : dya_oled_status_Animation_ANIMATION_OFF;
}

static enum dya_oled_status_animation runtime_animation(dya_oled_status_Animation animation) {
    return animation == dya_oled_status_Animation_ANIMATION_CYBER_FACE
               ? DYA_OLED_STATUS_ANIMATION_CYBER_FACE
               : DYA_OLED_STATUS_ANIMATION_OFF;
}

static void fill_config(dya_oled_status_OledStatusConfig *dst,
                        const struct dya_oled_status_runtime_config *src) {
    *dst = (dya_oled_status_OledStatusConfig)dya_oled_status_OledStatusConfig_init_zero;
    dst->orientation = proto_orientation(src->orientation);
    dst->animation = proto_animation(src->animation);
    dst->show_central_battery = src->show_central_battery;
    dst->show_peripheral_batteries = src->show_peripheral_batteries;
    dst->show_output = src->show_output;
    dst->show_layer = src->show_layer;
    dst->show_wpm = src->show_wpm;
    dst->show_modifiers = src->show_modifiers;
    dst->show_hid_indicators = src->show_hid_indicators;
}

static void fill_capabilities(dya_oled_status_Capabilities *dst) {
    *dst = (dya_oled_status_Capabilities)dya_oled_status_Capabilities_init_zero;
    dst->central_battery = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_CENTRAL);
    dst->peripheral_batteries = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL);
    dst->output = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_OUTPUT);
    dst->layer = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_LAYER);
    dst->wpm = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_WPM);
    dst->modifiers = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_MODIFIERS);
    dst->hid_indicators = IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_HID_INDICATORS);
    dst->portrait = true;
    dst->landscape = true;
    dst->animation_cyber_face = true;
}

static void sanitize_config(struct dya_oled_status_runtime_config *config) {
    if (config->animation != DYA_OLED_STATUS_ANIMATION_CYBER_FACE) {
        config->animation = DYA_OLED_STATUS_ANIMATION_OFF;
    }
    config->show_central_battery =
        config->show_central_battery && IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_CENTRAL);
    config->show_peripheral_batteries =
        config->show_peripheral_batteries && IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_BATTERY_PERIPHERAL);
    config->show_output = config->show_output && IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_OUTPUT);
    config->show_layer = config->show_layer && IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_LAYER);
    config->show_wpm = config->show_wpm && IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_WPM);
    config->show_modifiers = config->show_modifiers && IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_MODIFIERS);
    config->show_hid_indicators =
        config->show_hid_indicators && IS_ENABLED(CONFIG_DYA_OLED_STATUS_WIDGET_HID_INDICATORS);
}

static void config_from_proto(struct dya_oled_status_runtime_config *dst,
                              const dya_oled_status_OledStatusConfig *src) {
    dst->orientation = runtime_orientation(src->orientation);
    dst->animation = runtime_animation(src->animation);
    dst->show_central_battery = src->show_central_battery;
    dst->show_peripheral_batteries = src->show_peripheral_batteries;
    dst->show_output = src->show_output;
    dst->show_layer = src->show_layer;
    dst->show_wpm = src->show_wpm;
    dst->show_modifiers = src->show_modifiers;
    dst->show_hid_indicators = src->show_hid_indicators;
    sanitize_config(dst);
}

static int handle_get_config(dya_oled_status_Response *resp) {
    dya_oled_status_GetConfigResponse result = dya_oled_status_GetConfigResponse_init_zero;
    fill_config(&result.config, dya_oled_status_runtime_get());
    fill_capabilities(&result.capabilities);

    resp->which_response_type = dya_oled_status_Response_get_config_tag;
    resp->response_type.get_config = result;
    return 0;
}

static int handle_set_config(const dya_oled_status_SetConfigRequest *req,
                             dya_oled_status_Response *resp) {
    struct dya_oled_status_runtime_config config;
    config_from_proto(&config, &req->config);
    dya_oled_status_runtime_set(&config);
    dya_oled_status_display_refresh();

    dya_oled_status_SetConfigResponse result = dya_oled_status_SetConfigResponse_init_zero;
    fill_config(&result.config, dya_oled_status_runtime_get());

    resp->which_response_type = dya_oled_status_Response_set_config_tag;
    resp->response_type.set_config = result;
    return 0;
}

static int handle_reset_config(dya_oled_status_Response *resp) {
    dya_oled_status_runtime_reset();
    dya_oled_status_display_refresh();

    dya_oled_status_ResetConfigResponse result = dya_oled_status_ResetConfigResponse_init_zero;
    fill_config(&result.config, dya_oled_status_runtime_get());

    resp->which_response_type = dya_oled_status_Response_reset_config_tag;
    resp->response_type.reset_config = result;
    return 0;
}

static bool dya_oled_status_rpc_handle_request(const zmk_custom_CallRequest *raw_request,
                                               pb_callback_t *encode_response) {
    dya_oled_status_Response *resp =
        ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(dya__oled_status, encode_response);
    dya_oled_status_Request req = dya_oled_status_Request_init_zero;

    pb_istream_t req_stream =
        pb_istream_from_buffer(raw_request->payload.bytes, raw_request->payload.size);
    if (!pb_decode(&req_stream, dya_oled_status_Request_fields, &req)) {
        resp->which_response_type = dya_oled_status_Response_error_tag;
        snprintf(resp->response_type.error.message, sizeof(resp->response_type.error.message),
                 "Failed to decode request");
        LOG_WRN("Failed to decode DYA OLED status request: %s", PB_GET_ERROR(&req_stream));
        return true;
    }

    int rc = 0;
    switch (req.which_request_type) {
    case dya_oled_status_Request_get_config_tag:
        rc = handle_get_config(resp);
        break;
    case dya_oled_status_Request_set_config_tag:
        rc = handle_set_config(&req.request_type.set_config, resp);
        break;
    case dya_oled_status_Request_reset_config_tag:
        rc = handle_reset_config(resp);
        break;
    default:
        rc = -ENOTSUP;
        break;
    }

    if (rc != 0 && resp->which_response_type != dya_oled_status_Response_error_tag) {
        resp->which_response_type = dya_oled_status_Response_error_tag;
        snprintf(resp->response_type.error.message, sizeof(resp->response_type.error.message),
                 "Failed to process request: %d", rc);
    }

    return true;
}

