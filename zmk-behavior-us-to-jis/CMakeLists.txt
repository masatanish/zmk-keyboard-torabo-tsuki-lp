# SPDX-License-Identifier: MIT

if (CONFIG_ZMK_BEHAVIOR_US_TO_JIS AND ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL))
  target_sources(app PRIVATE
    src/behaviors/behavior_us_to_jis_toggle.c
    src/behaviors/behavior_us_to_jis_converter.c
  )
  zephyr_include_directories(include)
endif()
