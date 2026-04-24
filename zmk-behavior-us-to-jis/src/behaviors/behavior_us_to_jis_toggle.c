/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_us_to_jis_toggle

#include <errno.h>
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/behavior_us_to_jis.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static bool us_to_jis_enabled;

bool zmk_behavior_us_to_jis_is_enabled(void) { return us_to_jis_enabled; }

void zmk_behavior_us_to_jis_set_enabled(bool enabled) {
    us_to_jis_enabled = enabled;
    LOG_DBG("US to JIS conversion %s", enabled ? "enabled" : "disabled");
}

enum toggle_mode {
    ON,
    OFF,
};

struct behavior_us_to_jis_toggle_config {
    enum toggle_mode toggle_mode;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct behavior_us_to_jis_toggle_config *cfg =
        zmk_behavior_get_binding(binding->behavior_dev)->config;

    switch (cfg->toggle_mode) {
    case ON:
        zmk_behavior_us_to_jis_set_enabled(true);
        break;
    case OFF:
        zmk_behavior_us_to_jis_set_enabled(false);
        break;
    default:
        return -ENOTSUP;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_us_to_jis_toggle_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define US_TO_JIS_TOGGLE_INST(n)                                                                   \
    static const struct behavior_us_to_jis_toggle_config behavior_us_to_jis_toggle_config_##n = {  \
        .toggle_mode = DT_ENUM_IDX(DT_DRV_INST(n), toggle_mode),                                   \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &behavior_us_to_jis_toggle_config_##n, POST_KERNEL, \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_us_to_jis_toggle_driver_api);

DT_INST_FOREACH_STATUS_OKAY(US_TO_JIS_TOGGLE_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
