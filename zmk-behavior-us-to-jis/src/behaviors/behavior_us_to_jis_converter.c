/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Remaps US (type-pair) HID symbols to JIS (logical bit-pair) scancodes
 * for hosts configured as a Japanese keyboard. Based on the approach in
 * QMK us2jis + twpair_on_jis (eswai) and the Japanese JIS key defines in
 * QMK keymap_japanese.h.
 */

#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior_us_to_jis.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/modifiers.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/*
 * Prevent re-mapping events that we re-raise ourselves.
 * zmk_event_manager_raise() is synchronous, so a simple static guard works.
 */
static bool us_to_jis_reraising;
static uint8_t masked_shift_by_key[256];
static uint8_t masked_shift_active_count;

struct us2jis_row {
    uint16_t from;
    bool from_shift;
    uint16_t to;
    bool to_shift;
};

/* Based on QMK us2jis[] in twpair_on_jis.c */
static const struct us2jis_row us2jis_table[] = {
    /* KC_LPRN -> JP_LPRN */
    {HID_USAGE_KEY_KEYBOARD_9_AND_LEFT_PARENTHESIS, true, HID_USAGE_KEY_KEYBOARD_8_AND_ASTERISK,
     true},
    /* KC_RPRN -> JP_RPRN */
    {HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS, true,
     HID_USAGE_KEY_KEYBOARD_9_AND_LEFT_PARENTHESIS, true},
    /* KC_AT -> JP_AT */
    {HID_USAGE_KEY_KEYBOARD_2_AND_AT, true, HID_USAGE_KEY_KEYBOARD_LEFT_BRACKET_AND_LEFT_BRACE,
     false},
    /* KC_LBRC -> JP_LBRC */
    {HID_USAGE_KEY_KEYBOARD_LEFT_BRACKET_AND_LEFT_BRACE, false,
     HID_USAGE_KEY_KEYBOARD_RIGHT_BRACKET_AND_RIGHT_BRACE, false},
    /* KC_RBRC -> JP_RBRC */
    {HID_USAGE_KEY_KEYBOARD_RIGHT_BRACKET_AND_RIGHT_BRACE, false,
     HID_USAGE_KEY_KEYBOARD_NON_US_HASH_AND_TILDE, false},
    /* KC_LCBR -> JP_LCBR */
    {HID_USAGE_KEY_KEYBOARD_LEFT_BRACKET_AND_LEFT_BRACE, true,
     HID_USAGE_KEY_KEYBOARD_RIGHT_BRACKET_AND_RIGHT_BRACE, true},
    /* KC_RCBR -> JP_RCBR */
    {HID_USAGE_KEY_KEYBOARD_RIGHT_BRACKET_AND_RIGHT_BRACE, true,
     HID_USAGE_KEY_KEYBOARD_NON_US_HASH_AND_TILDE, true},
    /* KC_EQL -> JP_EQL */
    {HID_USAGE_KEY_KEYBOARD_EQUAL_AND_PLUS, false, HID_USAGE_KEY_KEYBOARD_MINUS_AND_UNDERSCORE, true},
    /* KC_BSLS -> JP_BSLS */
    {HID_USAGE_KEY_KEYBOARD_BACKSLASH_AND_PIPE, false,
     HID_USAGE_KEY_KEYBOARD_INTERNATIONAL1, false},
    /* KC_QUOT -> JP_QUOT */
    {HID_USAGE_KEY_KEYBOARD_APOSTROPHE_AND_QUOTE, false, HID_USAGE_KEY_KEYBOARD_7_AND_AMPERSAND,
     true},
    /* KC_GRV -> JP_GRV */
    {HID_USAGE_KEY_KEYBOARD_GRAVE_ACCENT_AND_TILDE, false,
     HID_USAGE_KEY_KEYBOARD_LEFT_BRACKET_AND_LEFT_BRACE, true},
    /* KC_PLUS -> JP_PLUS */
    {HID_USAGE_KEY_KEYBOARD_EQUAL_AND_PLUS, true, HID_USAGE_KEY_KEYBOARD_SEMICOLON_AND_COLON, true},
    /* KC_COLN -> JP_COLN */
    {HID_USAGE_KEY_KEYBOARD_SEMICOLON_AND_COLON, true, HID_USAGE_KEY_KEYBOARD_APOSTROPHE_AND_QUOTE,
     false},
    /* KC_UNDS -> JP_UNDS */
    {HID_USAGE_KEY_KEYBOARD_MINUS_AND_UNDERSCORE, true,
     HID_USAGE_KEY_KEYBOARD_INTERNATIONAL1, true},
    /* KC_PIPE -> JP_PIPE */
    {HID_USAGE_KEY_KEYBOARD_BACKSLASH_AND_PIPE, true,
     HID_USAGE_KEY_KEYBOARD_INTERNATIONAL3, true},
    /* KC_DQT -> JP_DQUO */
    {HID_USAGE_KEY_KEYBOARD_APOSTROPHE_AND_QUOTE, true, HID_USAGE_KEY_KEYBOARD_2_AND_AT, true},
    /* KC_ASTR -> JP_ASTR */
    {HID_USAGE_KEY_KEYBOARD_8_AND_ASTERISK, true, HID_USAGE_KEY_KEYBOARD_APOSTROPHE_AND_QUOTE,
     true},
    /* KC_TILD -> JP_TILD */
    {HID_USAGE_KEY_KEYBOARD_GRAVE_ACCENT_AND_TILDE, true, HID_USAGE_KEY_KEYBOARD_EQUAL_AND_PLUS,
     true},
    /* KC_AMPR -> JP_AMPR */
    {HID_USAGE_KEY_KEYBOARD_7_AND_AMPERSAND, true, HID_USAGE_KEY_KEYBOARD_6_AND_CARET, true},
    /* KC_CIRC -> JP_CIRC */
    {HID_USAGE_KEY_KEYBOARD_6_AND_CARET, true, HID_USAGE_KEY_KEYBOARD_EQUAL_AND_PLUS, false},
};

static void apply_target_shift(struct zmk_keycode_state_changed *out, bool to_shift,
                               zmk_mod_flags_t active_physical_shift) {
    out->implicit_modifiers &= (uint8_t) ~(MOD_LSFT | MOD_RSFT);
    out->explicit_modifiers &= (uint8_t) ~(MOD_LSFT | MOD_RSFT);

    if (!to_shift) {
        return;
    }

    if (active_physical_shift & MOD_LSFT) {
        out->implicit_modifiers |= MOD_LSFT;
    } else if (active_physical_shift & MOD_RSFT) {
        out->implicit_modifiers |= MOD_RSFT;
    } else {
        out->implicit_modifiers |= MOD_LSFT;
    }
}

static int AAA_us_to_jis_keycode(const zmk_event_t *eh) {
    if (!zmk_behavior_us_to_jis_is_enabled()) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (us_to_jis_reraising) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->usage_page != HID_USAGE_KEY) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->keycode >= HID_USAGE_KEY_KEYBOARD_LEFTCONTROL &&
        ev->keycode <= HID_USAGE_KEY_KEYBOARD_RIGHT_GUI) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    zmk_mod_flags_t physical_shift = zmk_hid_get_explicit_mods() & (MOD_LSFT | MOD_RSFT);
    zmk_mod_flags_t all_mods =
        (zmk_mod_flags_t)(ev->implicit_modifiers | ev->explicit_modifiers | physical_shift);
    bool from_shifted = (all_mods & (MOD_LSFT | MOD_RSFT)) != 0;

    for (size_t i = 0; i < ARRAY_SIZE(us2jis_table); i++) {
        const struct us2jis_row *m = &us2jis_table[i];
        if (m->from != ev->keycode || m->from_shift != from_shifted) {
            continue;
        }

        us_to_jis_reraising = true;
        struct zmk_keycode_state_changed out = *ev;
        out.keycode = m->to;
        apply_target_shift(&out, m->to_shift, physical_shift);

        if ((physical_shift & (MOD_LSFT | MOD_RSFT)) && !m->to_shift) {
            if (ev->state) {
                if (masked_shift_active_count == 0) {
                    zmk_hid_masked_modifiers_set(physical_shift);
                }
                if (ev->keycode < ARRAY_SIZE(masked_shift_by_key) &&
                    masked_shift_by_key[ev->keycode] < UINT8_MAX) {
                    masked_shift_by_key[ev->keycode]++;
                }
                if (masked_shift_active_count < UINT8_MAX) {
                    masked_shift_active_count++;
                }
            } else if (ev->keycode < ARRAY_SIZE(masked_shift_by_key) &&
                       masked_shift_by_key[ev->keycode] > 0) {
                masked_shift_by_key[ev->keycode]--;
                if (masked_shift_active_count > 0) {
                    masked_shift_active_count--;
                }
                if (masked_shift_active_count == 0) {
                    zmk_hid_masked_modifiers_clear();
                }
            }
        }

        raise_zmk_keycode_state_changed(out);
        us_to_jis_reraising = false;
        return ZMK_EV_EVENT_HANDLED;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(AAA_us_to_jis, AAA_us_to_jis_keycode);
ZMK_SUBSCRIPTION(AAA_us_to_jis, zmk_keycode_state_changed);
