/*****************************************************************************
 * MicroPython glue: exposes every driver function to Python
 ****************************************************************************/
#include "py/obj.h"
#include "py/runtime.h"
#include "st7565.h"

#ifndef STATIC
#define STATIC static
#endif


/* Single global instance (simplest pattern) */
STATIC st7565_t disp;

/* ───── Macros to cut boilerplate ─────────────────────────────────────── */
#define MP_DEFINE_WRAPPER_0(py_name,c_fn)                      \
    STATIC mp_obj_t py_name(void){ c_fn(&disp); return mp_const_none;} \
    STATIC MP_DEFINE_CONST_FUN_OBJ_0(py_name##_obj, py_name)

#define MP_DEFINE_WRAPPER_1(py_name,c_fn,conv)                             \
    STATIC mp_obj_t py_name(mp_obj_t a0){                                  \
        c_fn(&disp, conv(a0));                                             \
        return mp_const_none;                                              \
    }                                                                      \
    STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_name##_obj, py_name)

/* ───── Core: init / clear / draw ─────────────────────────────────────── */
STATIC mp_obj_t mp_st7565_init(size_t n, const mp_obj_t *a)
{
    if(n!=5) mp_raise_TypeError(MP_ERROR_TEXT("init(cs,rs,rst,sda,sck)"));
    st7565_init(&disp, SPI2_HOST,
        mp_obj_get_int(a[0]), mp_obj_get_int(a[1]), mp_obj_get_int(a[2]),
        mp_obj_get_int(a[3]), mp_obj_get_int(a[4]));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_st7565_init_obj,5,5,mp_st7565_init);

// STATIC mp_obj_t mp_st7565_draw(mp_obj_t buf_obj)
// {
//     mp_buffer_info_t bi;
//     mp_get_buffer_raise(buf_obj, &bi, MP_BUFFER_READ);
//     if (bi.len != ST7565_WIDTH*ST7565_PAGES)
//         mp_raise_ValueError(MP_ERROR_TEXT("buffer must be 1024 bytes"));
//     st7565_draw_buffer(&disp, (const uint8_t*)bi.buf);
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_st7565_draw_obj, mp_st7565_draw);
// STATIC mp_obj_t mp_st7565_draw(size_t n_args, const mp_obj_t *pos_args,
//                                 mp_map_t *kw_args)
// {
//     enum { ARG_buf, ARG_page, ARG_col, ARG_width, ARG_pages };
//     static const mp_arg_t allowed_args[] = {
//         { MP_QSTR_buf,   MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
//         { MP_QSTR_page,  MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0} },
//         { MP_QSTR_col,   MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0} },
//         { MP_QSTR_width, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = ST7565_WIDTH} },
//         { MP_QSTR_pages, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = ST7565_PAGES} },
//     };

//     mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
//     mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

//     mp_buffer_info_t bi;
//     mp_get_buffer_raise(args[ARG_buf].u_obj, &bi, MP_BUFFER_READ);

//     uint8_t page  = args[ARG_page].u_int;
//     uint8_t col   = args[ARG_col].u_int;
//     uint8_t width = args[ARG_width].u_int;
//     uint8_t pages = args[ARG_pages].u_int;

//     if (bi.len != width * pages) {
//         mp_raise_ValueError(MP_ERROR_TEXT("buffer size mismatch with width/pages"));
//     }

//     st7565_draw_buffer_ex(&disp, (const uint8_t *)bi.buf, page, col, width, pages);
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mp_st7565_draw_obj, 1, mp_st7565_draw);

STATIC mp_obj_t mp_st7565_draw(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_buf,    MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_page,   MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_column, MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_width,  MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = ST7565_WIDTH} },
        { MP_QSTR_pages,  MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = ST7565_PAGES} },
    };

    mp_arg_val_t args_parsed[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args_parsed);

    // Extract buffer
    mp_buffer_info_t bi;
    mp_get_buffer_raise(args_parsed[0].u_obj, &bi, MP_BUFFER_READ);

    // Sanity check
    int expected_size = args_parsed[3].u_int * args_parsed[4].u_int;
    if (bi.len != expected_size) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer size mismatch"));
    }

    // Call extended draw function
    st7565_draw_buffer_ex(&disp,
                          (const uint8_t *)bi.buf,
                          args_parsed[1].u_int,  // page
                          args_parsed[2].u_int,  // column
                          args_parsed[3].u_int,  // width
                          args_parsed[4].u_int); // pages

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mp_st7565_draw_obj, 1, mp_st7565_draw);


MP_DEFINE_WRAPPER_0(mp_st7565_clear , st7565_clear_display);

/* ───── Extra helpers (contrast / invert / power) ─────────────────────── */
MP_DEFINE_WRAPPER_1(mp_st7565_contrast, st7565_contrast , mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_invert , st7565_invert    , mp_obj_is_true);
MP_DEFINE_WRAPPER_0(mp_st7565_on     , st7565_power_on );
MP_DEFINE_WRAPPER_0(mp_st7565_off    , st7565_power_off);

/* ───── Low-level access (page / column / cmd / data) ─────────────────── */
MP_DEFINE_WRAPPER_1(mp_st7565_set_page , st7565_set_page_address  , mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_set_col  , st7565_set_column_address, mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_cmd      , st7565_write_instruction , mp_obj_get_int);
MP_DEFINE_WRAPPER_1(mp_st7565_data     , st7565_write_data        , mp_obj_get_int);

/* ───── Globals table ─────────────────────────────────────────────────── */
STATIC const mp_rom_map_elem_t st7565_globals[] = {
    /* High-level API */
    { MP_ROM_QSTR(MP_QSTR_init)   , MP_ROM_PTR(&mp_st7565_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear_display)  , MP_ROM_PTR(&mp_st7565_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_graphics)   , MP_ROM_PTR(&mp_st7565_draw_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_contrast),MP_ROM_PTR(&mp_st7565_contrast_obj)},
    { MP_ROM_QSTR(MP_QSTR_invert) , MP_ROM_PTR(&mp_st7565_invert_obj) },
    { MP_ROM_QSTR(MP_QSTR_on)     , MP_ROM_PTR(&mp_st7565_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_off)    , MP_ROM_PTR(&mp_st7565_off_obj) },

    /* Low-level helpers */
    { MP_ROM_QSTR(MP_QSTR_set_page_address), MP_ROM_PTR(&mp_st7565_set_page_obj)},
    { MP_ROM_QSTR(MP_QSTR_set_column_address),MP_ROM_PTR(&mp_st7565_set_col_obj)},
    { MP_ROM_QSTR(MP_QSTR_write_instruction)    , MP_ROM_PTR(&mp_st7565_cmd_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_data)   , MP_ROM_PTR(&mp_st7565_data_obj) },
};
STATIC MP_DEFINE_CONST_DICT(st7565_module_globals, st7565_globals);

/* ───── Module definition ─────────────────────────────────────────────── */
const mp_obj_module_t st7565_user_cmodule = {
    .base    = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&st7565_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_st7565, st7565_user_cmodule);
