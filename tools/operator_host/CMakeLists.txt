cmake_minimum_required(VERSION 3.20)

project(prospector_operator_renderer C)

set(PROSPECTOR_ROOT "" CACHE PATH "Prospector module root")
set(LVGL_DIR "" CACHE PATH "LVGL source directory")
set(OPERATOR_LAYER_COUNT "5" CACHE STRING "Layer count")
set(OPERATOR_PROFILE_COUNT "5" CACHE STRING "BLE profile count")
set(OPERATOR_PERIPHERAL_COUNT "2" CACHE STRING "Peripheral count")
set(OPERATOR_REGISTRY "" CACHE FILEPATH "Private generated host palette registry")

if(NOT PROSPECTOR_ROOT)
  message(FATAL_ERROR "PROSPECTOR_ROOT is required")
endif()

if(NOT LVGL_DIR)
  message(FATAL_ERROR "LVGL_DIR is required")
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/lv_conf.h ${CMAKE_BINARY_DIR}/lv_conf.h COPYONLY)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)

add_subdirectory(${LVGL_DIR} ${CMAKE_BINARY_DIR}/lvgl)
target_include_directories(lvgl SYSTEM PUBLIC ${CMAKE_CURRENT_LIST_DIR})

add_executable(operator_renderer
  src/main.c
  src/host_state.c
  src/host_modifier_order.c
  src/host_work.c
  src/host_session_stats.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/layouts/operator/status_screen.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/layouts/operator/background_texture.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/layouts/operator/operator_theme.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/layouts/operator/session_stats_widget.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/brightness_toast.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/layouts/operator/wpm_meter.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/layouts/operator/layer_display.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/layouts/operator/battery_circles.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/layouts/operator/output.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/fonts/FG_Medium_20.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/fonts/FG_Medium_21.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/fonts/FG_Medium_26.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/fonts/DINish_Expanded_Light_36.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/fonts/FR_Medium_32.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/fonts/DINish_Medium_24.c
)

add_executable(diagnostics_renderer
  src/diagnostics_main.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/session_stats.c
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/diagnostics_page.c
)

target_compile_definitions(operator_renderer PRIVATE
  CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR=1
  CONFIG_PROSPECTOR_OPERATOR_SESSION_STATS=1
  CONFIG_PROSPECTOR_MODIFIER_TOAST=1
  PROSPECTOR_HOST_DEFERRED_WORK=1
  CONFIG_PROSPECTOR_LAYER_NAME_UPPERCASE=1
  CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED=1
  PROSPECTOR_RENDER_INSTANT_BATTERY=1
  ZMK_BLE_PROFILE_COUNT=${OPERATOR_PROFILE_COUNT}
  ZMK_KEYMAP_LAYERS_LEN=${OPERATOR_LAYER_COUNT}
  PERIPHERAL_COUNT=${OPERATOR_PERIPHERAL_COUNT}
)

target_compile_definitions(diagnostics_renderer PRIVATE
  PROSPECTOR_RENDER_DIAGNOSTICS=1
  APP_VERSION_STRING="0.3.0"
  CONFIG_SYS_HEAP_RUNTIME_STATS=1
  CONFIG_LV_Z_MEM_POOL_SYS_HEAP=1
  CONFIG_LV_Z_MEM_POOL_SIZE=32768
)

target_compile_options(operator_renderer PRIVATE -include stdio.h)
if(OPERATOR_REGISTRY)
  if(NOT EXISTS "${OPERATOR_REGISTRY}")
    message(FATAL_ERROR "Private palette registry does not exist")
  endif()
  target_compile_options(operator_renderer PRIVATE "SHELL:-include \"${OPERATOR_REGISTRY}\"")
endif()
target_compile_options(diagnostics_renderer PRIVATE -include stdio.h)

target_include_directories(operator_renderer PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/include
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src/layouts/operator
  ${PROSPECTOR_ROOT}/include
)

target_include_directories(diagnostics_renderer PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/include
  ${PROSPECTOR_ROOT}/boards/shields/prospector_adapter/src
  ${PROSPECTOR_ROOT}/include
)

target_link_libraries(operator_renderer PRIVATE lvgl::lvgl m)
target_link_libraries(diagnostics_renderer PRIVATE lvgl::lvgl m)
