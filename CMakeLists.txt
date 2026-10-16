zephyr_include_directories(include)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
zephyr_library()

if(CONFIG_PROSPECTOR_POWER_STATUS AND NOT CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        zephyr_library_sources(src/power_status_peripheral.c)
endif()
if(CONFIG_PROSPECTOR_POWER_STATUS AND CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        zephyr_library_sources(src/power_status_central.c)
        zephyr_library_sources(src/power_status_format.c)
        zephyr_library_sources(src/events/peripheral_power_state_changed.c)
endif()

add_subdirectory_ifdef(CONFIG_PROSPECTOR_APDS9960 drivers/sensor/apds9960)

if(CONFIG_SHIELD_PROSPECTOR_ADAPTER)

        add_subdirectory(${ZEPHYR_CURRENT_MODULE_DIR}/drivers/display)

        if(CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED)
                zephyr_library_sources(src/events/caps_word_state_changed.c)
                set_source_files_properties(
                        ${APPLICATION_SOURCE_DIR}/src/behaviors/behavior_caps_word.c
                        TARGET_DIRECTORY app
                        PROPERTIES HEADER_FILE_ONLY ON)
                target_sources(app PRIVATE src/behaviors/behavior_caps_word.c)
        endif()

        if(CONFIG_DT_HAS_ZMK_BEHAVIOR_PROSPECTOR_BRIGHTNESS_ENABLED)
                target_sources(app PRIVATE src/behaviors/behavior_prospector_brightness.c)
        endif()

        if(CONFIG_DT_HAS_ZMK_BEHAVIOR_PROSPECTOR_DISPLAY_PAGE_ENABLED)
                target_sources(app PRIVATE src/behaviors/behavior_prospector_display_page.c)
        endif()

        if(CONFIG_PROSPECTOR_ENABLE_DIAGNOSTICS_PAGE OR CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS)
                # App library, ahead of ZMK's own sources, so physical presses are counted
                # before combo.c can consume them. The build helper verifies the order.
                target_sources(app PRIVATE src/session_press_counter.c)
        endif()

        zephyr_library_sources(src/events/split_central_status_changed.c)
        zephyr_library_sources(src/split/bluetooth/central_status_changed_observer.c)

endif()
