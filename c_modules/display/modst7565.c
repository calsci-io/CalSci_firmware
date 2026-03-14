/*****************************************************************************
 * MicroPython C module: st7565
 *
 * Complete ST7565R driver — all datasheet commands exposed to Python.
 * Usage:  import st7565
 *         st7565.init(cs, rs, rst, sda, sck)
 *         st7565.graphics(buffer)
 ****************************************************************************/
#include "py/obj.h"
#include "py/runtime.h"
#include "st7565.h"

static st7565_t disp;

static inline void mp_st7565_require_init(void)
{
    if (!disp.initialized) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("call init() first"));
    }
}

/* ───── Macros to cut boilerplate ─────────────────────────────────────────── */
#define MP_DEFINE_WRAPPER_0(py_name, c_fn)                                    \
    static mp_obj_t py_name(void) {                                           \
        mp_st7565_require_init();                                             \
        c_fn(&disp);                                                          \
        return mp_const_none;                                                 \
    }                                                                         \
    static MP_DEFINE_CONST_FUN_OBJ_0(py_name##_obj, py_name)

#define MP_DEFINE_WRAPPER_1(py_name, c_fn, conv)                              \
    static mp_obj_t py_name(mp_obj_t a0) {                                    \
        mp_st7565_require_init();                                             \
        c_fn(&disp, conv(a0));                                                \
        return mp_const_none;                                                 \
    }                                                                         \
    static MP_DEFINE_CONST_FUN_OBJ_1(py_name##_obj, py_name)

/* ───── init(cs, rs, rst, sda, sck, *, host=SPI2_HOST, ...) ─────────────── */
static mp_obj_t mp_st7565_init(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_cs,          MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_rs,          MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_rst,         MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_sda,         MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_sck,         MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_host,        MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = SPI2_HOST} },
        { MP_QSTR_contrast,    MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0x02} },
        { MP_QSTR_v0_ratio,    MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0x07} },
        { MP_QSTR_power_ctrl,  MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0x07} },
        { MP_QSTR_booster,     MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = ST7565_CMD_BOOSTER_2X_3X_4X} },
        { MP_QSTR_start_line,  MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_bias_1_7,    MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_adc_reverse, MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_com_reverse, MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = true} },
    };

    mp_arg_val_t parsed[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args,
                     MP_ARRAY_SIZE(allowed_args), allowed_args, parsed);

    mp_int_t host = parsed[5].u_int;
    mp_int_t contrast = parsed[6].u_int;
    mp_int_t v0_ratio = parsed[7].u_int;
    mp_int_t power_ctrl = parsed[8].u_int;
    mp_int_t booster = parsed[9].u_int;
    mp_int_t start_line = parsed[10].u_int;

    if (host != SPI2_HOST
#ifdef SPI3_HOST
        && host != SPI3_HOST
#endif
    ) {
#ifdef SPI3_HOST
        mp_raise_ValueError(MP_ERROR_TEXT("host must be SPI2_HOST or SPI3_HOST"));
#else
        mp_raise_ValueError(MP_ERROR_TEXT("host must be SPI2_HOST"));
#endif
    }
    if (contrast < 0 || contrast > 63) {
        mp_raise_ValueError(MP_ERROR_TEXT("contrast must be 0..63"));
    }
    if (v0_ratio < 0 || v0_ratio > 7) {
        mp_raise_ValueError(MP_ERROR_TEXT("v0_ratio must be 0..7"));
    }
    if (power_ctrl < 0 || power_ctrl > 7) {
        mp_raise_ValueError(MP_ERROR_TEXT("power_ctrl must be 0..7"));
    }
    if (booster != ST7565_CMD_BOOSTER_2X_3X_4X &&
        booster != ST7565_CMD_BOOSTER_5X &&
        booster != ST7565_CMD_BOOSTER_6X) {
        mp_raise_ValueError(MP_ERROR_TEXT("booster must be 0, 1, or 3"));
    }
    if (start_line < 0 || start_line > 63) {
        mp_raise_ValueError(MP_ERROR_TEXT("start_line must be 0..63"));
    }

    st7565_init_profile_t profile = {
        .contrast = (uint8_t)contrast,
        .v0_ratio = (uint8_t)v0_ratio,
        .power_ctrl = (uint8_t)power_ctrl,
        .booster = (uint8_t)booster,
        .start_line = (uint8_t)start_line,
        .bias_1_7 = parsed[11].u_bool,
        .adc_reverse = parsed[12].u_bool,
        .com_reverse = parsed[13].u_bool,
    };

    if (disp.initialized) {
        esp_err_t deinit_ret = st7565_deinit(&disp);
        if (deinit_ret != ESP_OK) {
            mp_raise_OSError(deinit_ret);
        }
    }

    esp_err_t ret = st7565_init_ex(&disp, (spi_host_device_t)host,
                                   parsed[0].u_int, parsed[1].u_int, parsed[2].u_int,
                                   parsed[3].u_int, parsed[4].u_int,
                                   &profile);

    if (ret != ESP_OK) {
        mp_raise_OSError(ret);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_st7565_init_obj, 5, mp_st7565_init);

/* ───── deinit() ─────────────────────────────────────────────────────────── */
static mp_obj_t mp_st7565_deinit(void)
{
    esp_err_t ret = st7565_deinit(&disp);
    if (ret != ESP_OK) {
        mp_raise_OSError(ret);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_st7565_deinit_obj, mp_st7565_deinit);

/* ───── graphics(buf, *, page=0, column=0, width=128, pages=8) ───────────── */
static mp_obj_t mp_st7565_draw(size_t n_args, const mp_obj_t *args,
                                mp_map_t *kw_args)
{
    mp_st7565_require_init();

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_buf,    MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_page,   MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_column, MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_width,  MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = ST7565_WIDTH} },
        { MP_QSTR_pages,  MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = ST7565_PAGES} },
    };

    mp_arg_val_t parsed[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, args, kw_args,
                     MP_ARRAY_SIZE(allowed_args), allowed_args, parsed);

    mp_buffer_info_t bi;
    mp_get_buffer_raise(parsed[0].u_obj, &bi, MP_BUFFER_READ);

    mp_int_t page = parsed[1].u_int;
    mp_int_t column = parsed[2].u_int;
    mp_int_t width = parsed[3].u_int;
    mp_int_t pages = parsed[4].u_int;

    if (page < 0 || page >= ST7565_PAGES) {
        mp_raise_ValueError(MP_ERROR_TEXT("page out of range"));
    }
    if (column < 0 || column >= ST7565_WIDTH) {
        mp_raise_ValueError(MP_ERROR_TEXT("column out of range"));
    }
    if (width <= 0 || width > ST7565_WIDTH) {
        mp_raise_ValueError(MP_ERROR_TEXT("width out of range"));
    }
    if (pages <= 0 || pages > ST7565_PAGES) {
        mp_raise_ValueError(MP_ERROR_TEXT("pages out of range"));
    }
    if (column + width > ST7565_WIDTH) {
        mp_raise_ValueError(MP_ERROR_TEXT("column + width exceeds display"));
    }
    if (page + pages > ST7565_PAGES) {
        mp_raise_ValueError(MP_ERROR_TEXT("page + pages exceeds display"));
    }

    mp_int_t expected = width * pages;
    if ((mp_int_t)bi.len != expected) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer size mismatch"));
    }

    st7565_draw_buffer_ex(&disp,
                          (const uint8_t *)bi.buf,
                          (uint8_t)page,
                          (uint8_t)column,
                          (uint8_t)width,
                          (uint8_t)pages);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_st7565_draw_obj, 1, mp_st7565_draw);

/* ───── Existing wrappers ────────────────────────────────────────────────── */
MP_DEFINE_WRAPPER_0(mp_st7565_clear,    st7565_clear_display);
MP_DEFINE_WRAPPER_1(mp_st7565_contrast, st7565_contrast,           mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_invert,   st7565_invert,             mp_obj_is_true);
MP_DEFINE_WRAPPER_0(mp_st7565_on,       st7565_power_on);
MP_DEFINE_WRAPPER_0(mp_st7565_off,      st7565_power_off);
MP_DEFINE_WRAPPER_1(mp_st7565_set_page, st7565_set_page_address,   mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_set_col,  st7565_set_column_address, mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_cmd,      st7565_write_instruction,  mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_data,     st7565_write_data,         mp_obj_get_int);

/* ───── NEW: display start line (hardware vertical scroll) ───────────────── */
MP_DEFINE_WRAPPER_1(mp_st7565_start_line, st7565_set_start_line,   mp_obj_get_int);

/* ───── NEW: all points on/off (display test / blanking) ─────────────────── */
MP_DEFINE_WRAPPER_1(mp_st7565_all_points, st7565_all_points_on,    mp_obj_is_true);

/* ───── NEW: sleep / wake (low-power mode) ───────────────────────────────── */
MP_DEFINE_WRAPPER_0(mp_st7565_sleep,     st7565_sleep);
MP_DEFINE_WRAPPER_0(mp_st7565_wake,      st7565_wake);

/* ───── NEW: software reset ──────────────────────────────────────────────── */
MP_DEFINE_WRAPPER_0(mp_st7565_reset,     st7565_reset);

/* ───── NEW: orientation (horizontal / vertical flip) ────────────────────── */
MP_DEFINE_WRAPPER_1(mp_st7565_set_adc,     st7565_set_adc,        mp_obj_is_true);
MP_DEFINE_WRAPPER_1(mp_st7565_set_com_dir, st7565_set_com_dir,    mp_obj_is_true);

/* ───── NEW: booster / bias / resistor ratio / power control ─────────────── */
MP_DEFINE_WRAPPER_1(mp_st7565_set_bias,       st7565_set_bias,       mp_obj_is_true);
MP_DEFINE_WRAPPER_1(mp_st7565_set_v0_ratio,   st7565_set_v0_ratio,   mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_set_booster,    st7565_set_booster,    mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_set_power_ctrl, st7565_set_power_ctrl, mp_obj_get_int);

/* ───── NEW: read-modify-write ───────────────────────────────────────────── */
MP_DEFINE_WRAPPER_0(mp_st7565_rmw_start, st7565_rmw_start);
MP_DEFINE_WRAPPER_0(mp_st7565_rmw_end,   st7565_rmw_end);

/* ───── NEW: nop ─────────────────────────────────────────────────────────── */
MP_DEFINE_WRAPPER_0(mp_st7565_nop,       st7565_nop);

/* ───── Module globals table ─────────────────────────────────────────────── */
static const mp_rom_map_elem_t st7565_globals[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),            MP_ROM_QSTR(MP_QSTR_st7565) },

    /* Lifecycle */
    { MP_ROM_QSTR(MP_QSTR_init),                MP_ROM_PTR(&mp_st7565_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit),              MP_ROM_PTR(&mp_st7565_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset),               MP_ROM_PTR(&mp_st7565_reset_obj) },

    /* Framebuffer */
    { MP_ROM_QSTR(MP_QSTR_clear_display),       MP_ROM_PTR(&mp_st7565_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_graphics),            MP_ROM_PTR(&mp_st7565_draw_obj) },

    /* Display control */
    { MP_ROM_QSTR(MP_QSTR_set_contrast),        MP_ROM_PTR(&mp_st7565_contrast_obj) },
    { MP_ROM_QSTR(MP_QSTR_invert),              MP_ROM_PTR(&mp_st7565_invert_obj) },
    { MP_ROM_QSTR(MP_QSTR_on),                  MP_ROM_PTR(&mp_st7565_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_off),                 MP_ROM_PTR(&mp_st7565_off_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_start_line),      MP_ROM_PTR(&mp_st7565_start_line_obj) },
    { MP_ROM_QSTR(MP_QSTR_all_points_on),       MP_ROM_PTR(&mp_st7565_all_points_obj) },

    /* Power management */
    { MP_ROM_QSTR(MP_QSTR_sleep),               MP_ROM_PTR(&mp_st7565_sleep_obj) },
    { MP_ROM_QSTR(MP_QSTR_wake),                MP_ROM_PTR(&mp_st7565_wake_obj) },

    /* Orientation */
    { MP_ROM_QSTR(MP_QSTR_set_adc),             MP_ROM_PTR(&mp_st7565_set_adc_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_com_dir),         MP_ROM_PTR(&mp_st7565_set_com_dir_obj) },

    /* Voltage / booster config */
    { MP_ROM_QSTR(MP_QSTR_set_bias),            MP_ROM_PTR(&mp_st7565_set_bias_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_v0_ratio),        MP_ROM_PTR(&mp_st7565_set_v0_ratio_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_booster),         MP_ROM_PTR(&mp_st7565_set_booster_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_power_ctrl),      MP_ROM_PTR(&mp_st7565_set_power_ctrl_obj) },

    /* Read-Modify-Write */
    { MP_ROM_QSTR(MP_QSTR_rmw_start),           MP_ROM_PTR(&mp_st7565_rmw_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmw_end),             MP_ROM_PTR(&mp_st7565_rmw_end_obj) },

    /* Low-level access */
    { MP_ROM_QSTR(MP_QSTR_set_page_address),    MP_ROM_PTR(&mp_st7565_set_page_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_column_address),  MP_ROM_PTR(&mp_st7565_set_col_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_instruction),   MP_ROM_PTR(&mp_st7565_cmd_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_data),          MP_ROM_PTR(&mp_st7565_data_obj) },
    { MP_ROM_QSTR(MP_QSTR_nop),                 MP_ROM_PTR(&mp_st7565_nop_obj) },

    /* Constants */
    { MP_ROM_QSTR(MP_QSTR_WIDTH),               MP_ROM_INT(ST7565_WIDTH) },
    { MP_ROM_QSTR(MP_QSTR_HEIGHT),              MP_ROM_INT(ST7565_HEIGHT) },
    { MP_ROM_QSTR(MP_QSTR_PAGES),               MP_ROM_INT(ST7565_PAGES) },
    { MP_ROM_QSTR(MP_QSTR_SPI2_HOST),           MP_ROM_INT(SPI2_HOST) },
#ifdef SPI3_HOST
    { MP_ROM_QSTR(MP_QSTR_SPI3_HOST),           MP_ROM_INT(SPI3_HOST) },
#endif
};
static MP_DEFINE_CONST_DICT(st7565_module_globals, st7565_globals);

/* ───── Module definition ────────────────────────────────────────────────── */
const mp_obj_module_t st7565_user_cmodule = {
    .base    = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&st7565_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_st7565, st7565_user_cmodule);
