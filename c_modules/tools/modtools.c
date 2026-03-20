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

#include "../calsci_log/calsci_log.h"

#include "py/obj.h"
#include "py/objstr.h"
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

static const MP_DEFINE_STR_OBJ(tools_doc_module_obj,
    "CalSci helper module for framebuffer upload utilities.\n"
    "\n"
    "Use refresh(...) to wrap display.graphics so small changes are sent as\n"
    "page-sized partial updates instead of full-frame uploads.");
static const MP_DEFINE_STR_OBJ(tools_doc_refresh_obj,
    "Wrap a graphics(buf, ...) function and auto-send only dirty page spans until the threshold is reached.");
static const MP_DEFINE_STR_OBJ(tools_wrapper_doc_obj,
    "Callable wrapper returned by tools.refresh(...). It forwards graphics(...) calls and decides between partial and full refresh.");
static const MP_DEFINE_STR_OBJ(tools_wrapper_doc_reset_obj,
    "Forget the previous framebuffer snapshot so the next call sends a full frame.");
static const MP_DEFINE_STR_OBJ(tools_wrapper_doc_pixels_changed_obj,
    "pixels_changed([value]) -> get or set the full-refresh pixel threshold.");

typedef struct _tools_help_entry_t {
    qstr name;
    const mp_obj_str_t *doc;
} tools_help_entry_t;

static const tools_help_entry_t tools_help_entries[] = {
    { MP_QSTR_refresh, &tools_doc_refresh_obj },
};

static const tools_help_entry_t tools_wrapper_help_entries[] = {
    { MP_QSTR_reset, &tools_wrapper_doc_reset_obj },
    { MP_QSTR_pixels_changed, &tools_wrapper_doc_pixels_changed_obj },
};

static void tools_help_print_entry(const tools_help_entry_t *entry) {
    mp_print_str(MP_PYTHON_PRINTER, "  ");
    mp_obj_print(MP_OBJ_NEW_QSTR(entry->name), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, " -- ");
    mp_obj_print(MP_OBJ_FROM_PTR(entry->doc), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
}

static void tools_help_print_all(void) {
    mp_obj_print(MP_OBJ_FROM_PTR(&tools_doc_module_obj), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
    for (size_t i = 0; i < MP_ARRAY_SIZE(tools_help_entries); ++i) {
        tools_help_print_entry(&tools_help_entries[i]);
    }
}

static void tools_wrapper_help_print_all(void) {
    mp_obj_print(MP_OBJ_FROM_PTR(&tools_wrapper_doc_obj), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
    for (size_t i = 0; i < MP_ARRAY_SIZE(tools_wrapper_help_entries); ++i) {
        tools_help_print_entry(&tools_wrapper_help_entries[i]);
    }
}

static mp_obj_t tools_help(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        tools_help_print_all();
        return mp_const_none;
    }

    qstr topic = mp_obj_str_get_qstr(args[0]);
    for (size_t i = 0; i < MP_ARRAY_SIZE(tools_help_entries); ++i) {
        if (tools_help_entries[i].name == topic) {
            mp_obj_print(MP_OBJ_FROM_PTR(tools_help_entries[i].doc), PRINT_STR);
            mp_print_str(MP_PYTHON_PRINTER, "\n");
            return mp_const_none;
        }
    }

    mp_raise_ValueError(MP_ERROR_TEXT("unknown tools help topic"));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tools_help_obj, 0, 1, tools_help);

static mp_obj_t tools_refresh_wrapper_help(size_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        tools_wrapper_help_print_all();
        return mp_const_none;
    }

    qstr topic = mp_obj_str_get_qstr(args[1]);
    for (size_t i = 0; i < MP_ARRAY_SIZE(tools_wrapper_help_entries); ++i) {
        if (tools_wrapper_help_entries[i].name == topic) {
            mp_obj_print(MP_OBJ_FROM_PTR(tools_wrapper_help_entries[i].doc), PRINT_STR);
            mp_print_str(MP_PYTHON_PRINTER, "\n");
            return mp_const_none;
        }
    }

    mp_raise_ValueError(MP_ERROR_TEXT("unknown refresh_wrapper help topic"));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tools_refresh_wrapper_help_obj, 1, 2, tools_refresh_wrapper_help);

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
    { MP_ROM_QSTR(MP_QSTR_help), MP_ROM_PTR(&tools_refresh_wrapper_help_obj) },
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

    calsci_log_writef("INFO", "tools", "refresh wrapper created (pixels_changed=%d)",
        (int)self->pixels_changed_threshold);

    return MP_OBJ_FROM_PTR(self);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(tools_refresh_obj, 1, tools_refresh);

static const mp_rom_map_elem_t tools_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tools) },
    { MP_ROM_QSTR(MP_QSTR_help), MP_ROM_PTR(&tools_help_obj) },
    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&tools_refresh_obj) },
};
static MP_DEFINE_CONST_DICT(tools_module_globals, tools_module_globals_table);

const mp_obj_module_t tools_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&tools_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tools, tools_user_cmodule);
