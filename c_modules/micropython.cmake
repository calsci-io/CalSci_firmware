include(${MICROPY_DIR}/c_modules/display/micropython.cmake)
include(${MICROPY_DIR}/c_modules/tools/micropython.cmake)
include(${MICROPY_DIR}/c_modules/hybrid_sim/micropython.cmake)
include(${MICROPY_DIR}/c_modules/calsci_runtime/micropython.cmake)

# Inject CalSci USB/product and runtime prompt behavior without touching core sources.
add_compile_options(
    -include
    ${MICROPY_DIR}/c_modules/calsci_runtime/calsci_runtime.h
)

# LVGL bindings are fully self-contained under c_modules/lvgl.
set(LV_BINDING_DIR "${MICROPY_DIR}/c_modules/lvgl/lv_binding_micropython")

if(NOT DEFINED LV_CONF_PATH)
    set(LV_CONF_PATH "${LV_BINDING_DIR}/lv_conf.h")
endif()

if(NOT DEFINED LV_CFLAGS)
    set(LV_CFLAGS "-DLV_COLOR_DEPTH=1")
endif()

include(${LV_BINDING_DIR}/micropython.cmake)
