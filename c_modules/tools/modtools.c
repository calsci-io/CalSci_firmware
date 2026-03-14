/*****************************************************************************
 * MicroPython C module: tools
 *
 * Simple partial-refresh wrapper for ST7565-style frame uploads.
 *
 * Usage:
 *   import tools, st7565 as display
 *   display.graphics = tools.refresh(display.graphics, pixels_changed=200)
 *   display.graphics(fb_buf)  # auto full/partial decision
 ****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "py/obj.h"
#include "py/runtime.h"

#define TOOLS_REFRESH_WIDTH (128)
#define TOOLS_REFRESH_PAGES (8)

typedef struct _tools_refresh_wrapper_obj_t {
    mp_obj_base_t base;
    mp_obj_t graphics_fun;
    size_t width;
    size_t pages;
    size_t buf_len;
    size_t pixels_changed_threshold;
    bool has_snapshot;
    uint8_t *snapshot;
    mp_int_t *dirty_min;
    mp_int_t *dirty_max;
} tools_refresh_wrapper_obj_t;

static inline mp_obj_t tools_call_graphics_region(mp_obj_t graphics_fun, mp_obj_t buf_obj, mp_int_t page, mp_int_t column, mp_int_t width, mp_int_t pages) {
    mp_obj_t args[9] = {
        buf_obj,
        MP_OBJ_NEW_QSTR(MP_QSTR_page), mp_obj_new_int(page),
        MP_OBJ_NEW_QSTR(MP_QSTR_column), mp_obj_new_int(column),
        MP_OBJ_NEW_QSTR(MP_QSTR_width), mp_obj_new_int(width),
        MP_OBJ_NEW_QSTR(MP_QSTR_pages), mp_obj_new_int(pages),
    };
    return mp_call_function_n_kw(graphics_fun, 1, 4, args);
}

static mp_obj_t tools_refresh_wrapper_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    tools_refresh_wrapper_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // Preserve original behavior for non-simple/full-buffer call forms.
    if (n_args != 1 || n_kw != 0) {
        return mp_call_function_n_kw(self->graphics_fun, n_args, n_kw, args);
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[0], &bufinfo, MP_BUFFER_READ);

    // If this isn't the configured full framebuffer buffer size, forward it.
    if (bufinfo.len != self->buf_len) {
        return mp_call_function_n_kw(self->graphics_fun, n_args, n_kw, args);
    }

    const uint8_t *buf = (const uint8_t *)bufinfo.buf;

    // First frame should be uploaded as full frame.
    if (!self->has_snapshot) {
        mp_obj_t ret = mp_call_function_n_kw(self->graphics_fun, 1, 0, args);
        memcpy(self->snapshot, buf, self->buf_len);
        self->has_snapshot = true;
        return ret;
    }

    size_t changed_pixels = 0;
    size_t dirty_rows = 0;
    size_t idx = 0;
    for (size_t p = 0; p < self->pages; ++p) {
        self->dirty_min[p] = (mp_int_t)self->width;
        self->dirty_max[p] = -1;
        for (size_t c = 0; c < self->width; ++c, ++idx) {
            uint8_t oldv = self->snapshot[idx];
            uint8_t newv = buf[idx];
            uint8_t diff = oldv ^ newv;
            if (diff != 0) {
                changed_pixels += (size_t)__builtin_popcount((unsigned int)diff);
                if ((mp_int_t)c < self->dirty_min[p]) {
                    self->dirty_min[p] = (mp_int_t)c;
                }
                if ((mp_int_t)c > self->dirty_max[p]) {
                    self->dirty_max[p] = (mp_int_t)c;
                }
            }
        }
        if (self->dirty_max[p] >= self->dirty_min[p]) {
            ++dirty_rows;
        }
    }

    if (dirty_rows == 0) {
        return mp_const_none;
    }

    if (changed_pixels >= self->pixels_changed_threshold) {
        mp_obj_t ret = mp_call_function_n_kw(self->graphics_fun, 1, 0, args);
        memcpy(self->snapshot, buf, self->buf_len);
        return ret;
    }

    for (size_t p = 0; p < self->pages; ++p) {
        if (self->dirty_max[p] < self->dirty_min[p]) {
            continue;
        }
        size_t col = (size_t)self->dirty_min[p];
        size_t span = (size_t)(self->dirty_max[p] - self->dirty_min[p] + 1);
        size_t off = p * self->width + col;
        mp_obj_t row_buf = mp_obj_new_memoryview('B', span, (void *)(buf + off));
        (void)tools_call_graphics_region(self->graphics_fun, row_buf, (mp_int_t)p, (mp_int_t)col, (mp_int_t)span, 1);
    }

    memcpy(self->snapshot, buf, self->buf_len);
    return mp_const_none;
}

static mp_obj_t tools_refresh_wrapper_reset(mp_obj_t self_in) {
    tools_refresh_wrapper_obj_t *self = MP_OBJ_TO_PTR(self_in);
    self->has_snapshot = false;
    memset(self->snapshot, 0, self->buf_len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(tools_refresh_wrapper_reset_obj, tools_refresh_wrapper_reset);

static mp_obj_t tools_refresh_wrapper_pixels_changed(size_t n_args, const mp_obj_t *args) {
    tools_refresh_wrapper_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (n_args == 1) {
        return mp_obj_new_int_from_uint(self->pixels_changed_threshold);
    }
    mp_int_t v = mp_obj_get_int(args[1]);
    if (v < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("pixels_changed must be >= 0"));
    }
    self->pixels_changed_threshold = (size_t)v;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tools_refresh_wrapper_pixels_changed_obj, 1, 2, tools_refresh_wrapper_pixels_changed);

static const mp_rom_map_elem_t tools_refresh_wrapper_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&tools_refresh_wrapper_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_pixels_changed), MP_ROM_PTR(&tools_refresh_wrapper_pixels_changed_obj) },
};
static MP_DEFINE_CONST_DICT(tools_refresh_wrapper_locals_dict, tools_refresh_wrapper_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    tools_type_refresh_wrapper,
    MP_QSTR_refresh_wrapper,
    MP_TYPE_FLAG_NONE,
    call, tools_refresh_wrapper_call,
    locals_dict, &tools_refresh_wrapper_locals_dict
    );

static mp_obj_t tools_refresh(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum {
        ARG_graphics,
        ARG_pixels_changed,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_graphics, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_pixels_changed, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 200} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (!mp_obj_is_callable(args[ARG_graphics].u_obj)) {
        mp_raise_TypeError(MP_ERROR_TEXT("graphics must be callable"));
    }
    if (args[ARG_pixels_changed].u_int < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("pixels_changed must be >= 0"));
    }

    tools_refresh_wrapper_obj_t *self = mp_obj_malloc(tools_refresh_wrapper_obj_t, &tools_type_refresh_wrapper);
    self->graphics_fun = args[ARG_graphics].u_obj;
    self->width = TOOLS_REFRESH_WIDTH;
    self->pages = TOOLS_REFRESH_PAGES;
    self->buf_len = self->width * self->pages;
    self->pixels_changed_threshold = (size_t)args[ARG_pixels_changed].u_int;
    self->has_snapshot = false;
    self->snapshot = m_new(uint8_t, self->buf_len);
    self->dirty_min = m_new(mp_int_t, self->pages);
    self->dirty_max = m_new(mp_int_t, self->pages);
    memset(self->snapshot, 0, self->buf_len);

    return MP_OBJ_FROM_PTR(self);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(tools_refresh_obj, 1, tools_refresh);

static const mp_rom_map_elem_t tools_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tools) },
    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&tools_refresh_obj) },
};
static MP_DEFINE_CONST_DICT(tools_module_globals, tools_module_globals_table);

const mp_obj_module_t tools_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&tools_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tools, tools_user_cmodule);
