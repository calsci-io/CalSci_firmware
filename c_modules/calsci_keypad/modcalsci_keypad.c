/*****************************************************************************
 * MicroPython C module: calsci_keypad
 *
 * Configurable matrix keypad scanner for CalSci firmware.
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../calsci_runtime/calsci_runtime.h"

#include "driver/gpio.h"

#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/mphal.h"

#include "mphalport.h"

#define CALSCI_KEYPAD_PULL_NONE (0)
#define CALSCI_KEYPAD_PULL_UP   (1)
#define CALSCI_KEYPAD_PULL_DOWN (2)

typedef struct _calsci_keypad_obj_t {
    mp_obj_base_t base;
    mp_obj_t rows_obj;
    mp_obj_t cols_obj;
    mp_hal_pin_obj_t *rows;
    mp_hal_pin_obj_t *cols;
    size_t row_count;
    size_t col_count;
    mp_uint_t poll_delay_ms;
    uint8_t row_idle_level;
    uint8_t row_active_level;
    uint8_t pressed_level;
    gpio_pull_mode_t pull_mode;
    bool state;
} calsci_keypad_obj_t;

static bool calsci_keypad_print_presses = false;

static const MP_DEFINE_STR_OBJ(calsci_keypad_doc_module_obj,
    "CalSci keypad helpers.\n"
    "\n"
    "Create a Keypad(rows, cols, ...) object with runtime pin lists so the\n"
    "matrix scanner lives in firmware while board wiring stays configurable.");
static const MP_DEFINE_STR_OBJ(calsci_keypad_doc_keypad_obj,
    "Keypad(rows, cols, *, row_active=0, row_idle=1, pull=PULL_UP, pressed_level=0, poll_delay_ms=5)");
static const MP_DEFINE_STR_OBJ(calsci_keypad_doc_set_print_obj,
    "set_keypad_press_printing(enabled) -> enable or disable debug printing of (col, row) hits.");
static const MP_DEFINE_STR_OBJ(calsci_keypad_doc_print_state_obj,
    "keypad_press_printing() -> return current debug-print flag.");
static const MP_DEFINE_STR_OBJ(calsci_keypad_doc_pull_none_obj,
    "Disable internal pull resistors on column inputs.");
static const MP_DEFINE_STR_OBJ(calsci_keypad_doc_pull_up_obj,
    "Use internal pull-up on column inputs.");
static const MP_DEFINE_STR_OBJ(calsci_keypad_doc_pull_down_obj,
    "Use internal pull-down on column inputs.");

typedef struct _calsci_keypad_help_entry_t {
    qstr name;
    const mp_obj_str_t *doc;
} calsci_keypad_help_entry_t;

static const calsci_keypad_help_entry_t calsci_keypad_help_entries[] = {
    { MP_QSTR_Keypad, &calsci_keypad_doc_keypad_obj },
    { MP_QSTR_set_keypad_press_printing, &calsci_keypad_doc_set_print_obj },
    { MP_QSTR_keypad_press_printing, &calsci_keypad_doc_print_state_obj },
    { MP_QSTR_PULL_NONE, &calsci_keypad_doc_pull_none_obj },
    { MP_QSTR_PULL_UP, &calsci_keypad_doc_pull_up_obj },
    { MP_QSTR_PULL_DOWN, &calsci_keypad_doc_pull_down_obj },
};

static void calsci_keypad_help_print_entry(const calsci_keypad_help_entry_t *entry) {
    mp_print_str(MP_PYTHON_PRINTER, "  ");
    mp_obj_print(MP_OBJ_NEW_QSTR(entry->name), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, " -- ");
    mp_obj_print(MP_OBJ_FROM_PTR(entry->doc), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
}

static void calsci_keypad_help_print_all(void) {
    mp_obj_print(MP_OBJ_FROM_PTR(&calsci_keypad_doc_module_obj), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
    for (size_t i = 0; i < MP_ARRAY_SIZE(calsci_keypad_help_entries); ++i) {
        calsci_keypad_help_print_entry(&calsci_keypad_help_entries[i]);
    }
}

static mp_obj_t calsci_keypad_help(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        calsci_keypad_help_print_all();
        return mp_const_none;
    }

    qstr topic = mp_obj_str_get_qstr(args[0]);
    for (size_t i = 0; i < MP_ARRAY_SIZE(calsci_keypad_help_entries); ++i) {
        if (calsci_keypad_help_entries[i].name == topic) {
            mp_obj_print(MP_OBJ_FROM_PTR(calsci_keypad_help_entries[i].doc), PRINT_STR);
            mp_print_str(MP_PYTHON_PRINTER, "\n");
            return mp_const_none;
        }
    }

    mp_raise_ValueError(MP_ERROR_TEXT("unknown calsci_keypad help topic"));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(calsci_keypad_help_obj, 0, 1, calsci_keypad_help);

static gpio_pull_mode_t calsci_keypad_parse_pull_mode(mp_int_t pull_mode) {
    switch (pull_mode) {
        case CALSCI_KEYPAD_PULL_NONE:
            return GPIO_FLOATING;
        case CALSCI_KEYPAD_PULL_UP:
            return GPIO_PULLUP_ONLY;
        case CALSCI_KEYPAD_PULL_DOWN:
            return GPIO_PULLDOWN_ONLY;
        default:
            mp_raise_ValueError(MP_ERROR_TEXT("pull must be PULL_NONE, PULL_UP, or PULL_DOWN"));
    }
}

static void calsci_keypad_load_pin_array(mp_obj_t pins_in, mp_obj_t *pins_obj_out, mp_hal_pin_obj_t **pins_out, size_t *len_out) {
    size_t len = 0;
    mp_obj_t *items = NULL;
    mp_obj_get_array(pins_in, &len, &items);
    if (len == 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("pin list must not be empty"));
    }

    *pins_obj_out = mp_obj_new_tuple(len, items);
    *pins_out = m_new(mp_hal_pin_obj_t, len);
    *len_out = len;

    for (size_t i = 0; i < len; ++i) {
        (*pins_out)[i] = machine_pin_get_id(items[i]);
    }
}

static void calsci_keypad_configure_columns(const calsci_keypad_obj_t *self) {
    for (size_t i = 0; i < self->col_count; ++i) {
        mp_hal_pin_input(self->cols[i]);
        gpio_set_pull_mode(self->cols[i], self->pull_mode);
    }
}

static void calsci_keypad_release_rows(calsci_keypad_obj_t *self) {
    for (size_t i = 0; i < self->row_count; ++i) {
        mp_hal_pin_output(self->rows[i]);
        mp_hal_pin_write(self->rows[i], self->row_idle_level);
    }
}

static void calsci_keypad_release_rows_cb(void *ctx) {
    calsci_keypad_release_rows((calsci_keypad_obj_t *)ctx);
}

static void calsci_keypad_wait_if_blocked(calsci_keypad_obj_t *self) {
    calsci_runtime_wait_if_keypad_blocked(calsci_keypad_release_rows_cb, self);
}

static void calsci_keypad_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    calsci_keypad_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "Keypad(rows=%u, cols=%u, state=%d, poll_delay_ms=%u)",
        (unsigned)self->row_count, (unsigned)self->col_count, self->state, (unsigned)self->poll_delay_ms);
}

static mp_obj_t calsci_keypad_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_rows,
        ARG_cols,
        ARG_row_active,
        ARG_row_idle,
        ARG_pull,
        ARG_pressed_level,
        ARG_poll_delay_ms,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_rows, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_cols, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_row_active, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_row_idle, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_pull, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = CALSCI_KEYPAD_PULL_UP} },
        { MP_QSTR_pressed_level, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_poll_delay_ms, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 5} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t row_active = args[ARG_row_active].u_int;
    mp_int_t row_idle = args[ARG_row_idle].u_int;
    mp_int_t pull = args[ARG_pull].u_int;
    mp_int_t pressed_level = args[ARG_pressed_level].u_int;
    mp_int_t poll_delay_ms = args[ARG_poll_delay_ms].u_int;

    if ((row_active != 0 && row_active != 1) || (row_idle > 1)) {
        mp_raise_ValueError(MP_ERROR_TEXT("row levels must be 0 or 1"));
    }
    if (row_idle < 0) {
        row_idle = row_active == 0 ? 1 : 0;
    }
    if (row_idle != 0 && row_idle != 1) {
        mp_raise_ValueError(MP_ERROR_TEXT("row levels must be 0 or 1"));
    }
    if (row_idle == row_active) {
        mp_raise_ValueError(MP_ERROR_TEXT("row_active and row_idle must differ"));
    }
    if (pressed_level < 0) {
        pressed_level = pull == CALSCI_KEYPAD_PULL_DOWN ? 1 : 0;
    }
    if (pressed_level != 0 && pressed_level != 1) {
        mp_raise_ValueError(MP_ERROR_TEXT("pressed_level must be 0 or 1"));
    }
    if (poll_delay_ms < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("poll_delay_ms must be >= 0"));
    }

    calsci_keypad_obj_t *self = mp_obj_malloc(calsci_keypad_obj_t, type);
    self->rows_obj = mp_const_none;
    self->cols_obj = mp_const_none;
    self->rows = NULL;
    self->cols = NULL;
    self->row_count = 0;
    self->col_count = 0;
    self->row_idle_level = (uint8_t)row_idle;
    self->row_active_level = (uint8_t)row_active;
    self->pressed_level = (uint8_t)pressed_level;
    self->poll_delay_ms = (mp_uint_t)poll_delay_ms;
    self->pull_mode = calsci_keypad_parse_pull_mode(pull);
    self->state = true;

    calsci_keypad_load_pin_array(args[ARG_rows].u_obj, &self->rows_obj, &self->rows, &self->row_count);
    calsci_keypad_load_pin_array(args[ARG_cols].u_obj, &self->cols_obj, &self->cols, &self->col_count);
    calsci_keypad_configure_columns(self);
    calsci_keypad_release_rows(self);

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t calsci_keypad_keypad_loop(mp_obj_t self_in) {
    calsci_keypad_obj_t *self = MP_OBJ_TO_PTR(self_in);
    while (self->state) {
        calsci_keypad_wait_if_blocked(self);
        for (size_t row = 0; row < self->row_count; ++row) {
            calsci_keypad_wait_if_blocked(self);
            mp_hal_pin_output(self->rows[row]);
            mp_hal_pin_write(self->rows[row], self->row_active_level);
            for (size_t col = 0; col < self->col_count; ++col) {
                calsci_keypad_wait_if_blocked(self);
                if (mp_hal_pin_read(self->cols[col]) == self->pressed_level) {
                    mp_hal_pin_write(self->rows[row], self->row_idle_level);
                    if (calsci_keypad_print_presses) {
                        mp_printf(&mp_plat_print, "(%u, %u)\n", (unsigned)col, (unsigned)row);
                    }
                    mp_obj_t tuple[2] = {
                        mp_obj_new_int_from_uint(col),
                        mp_obj_new_int_from_uint(row),
                    };
                    return mp_obj_new_tuple(2, tuple);
                }
            }
            mp_hal_pin_write(self->rows[row], self->row_idle_level);
        }
        mp_hal_delay_ms(self->poll_delay_ms);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(calsci_keypad_keypad_loop_obj, calsci_keypad_keypad_loop);

static mp_obj_t calsci_keypad_keypad_stop(mp_obj_t self_in) {
    calsci_keypad_obj_t *self = MP_OBJ_TO_PTR(self_in);
    self->state = false;
    calsci_keypad_release_rows(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(calsci_keypad_keypad_stop_obj, calsci_keypad_keypad_stop);

static mp_obj_t calsci_keypad_keypad_start(mp_obj_t self_in) {
    calsci_keypad_obj_t *self = MP_OBJ_TO_PTR(self_in);
    self->state = true;
    calsci_keypad_release_rows(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(calsci_keypad_keypad_start_obj, calsci_keypad_keypad_start);

static void calsci_keypad_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    calsci_keypad_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (dest[0] != MP_OBJ_NULL) {
        if (attr == MP_QSTR_state) {
            self->state = mp_obj_is_true(dest[0]);
            if (!self->state) {
                calsci_keypad_release_rows(self);
            }
            dest[0] = MP_OBJ_NULL;
            return;
        }
        dest[1] = MP_OBJ_SENTINEL;
        return;
    }

    if (attr == MP_QSTR_rows) {
        dest[0] = self->rows_obj;
        return;
    }
    if (attr == MP_QSTR_cols) {
        dest[0] = self->cols_obj;
        return;
    }
    if (attr == MP_QSTR_state) {
        dest[0] = mp_obj_new_bool(self->state);
        return;
    }
    if (attr == MP_QSTR_poll_delay_ms) {
        dest[0] = mp_obj_new_int_from_uint(self->poll_delay_ms);
        return;
    }
    if (attr == MP_QSTR_row_active) {
        dest[0] = mp_obj_new_int(self->row_active_level);
        return;
    }
    if (attr == MP_QSTR_row_idle) {
        dest[0] = mp_obj_new_int(self->row_idle_level);
        return;
    }
    if (attr == MP_QSTR_pressed_level) {
        dest[0] = mp_obj_new_int(self->pressed_level);
        return;
    }

    dest[1] = MP_OBJ_SENTINEL;
}

static const mp_rom_map_elem_t calsci_keypad_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_keypad_loop), MP_ROM_PTR(&calsci_keypad_keypad_loop_obj) },
    { MP_ROM_QSTR(MP_QSTR_keypad_stop), MP_ROM_PTR(&calsci_keypad_keypad_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_keypad_start), MP_ROM_PTR(&calsci_keypad_keypad_start_obj) },
};
static MP_DEFINE_CONST_DICT(calsci_keypad_locals_dict, calsci_keypad_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    calsci_keypad_type,
    MP_QSTR_Keypad,
    MP_TYPE_FLAG_NONE,
    make_new, calsci_keypad_make_new,
    print, calsci_keypad_print,
    attr, calsci_keypad_attr,
    locals_dict, &calsci_keypad_locals_dict
    );

static mp_obj_t calsci_keypad_set_press_printing(mp_obj_t enabled_in) {
    calsci_keypad_print_presses = mp_obj_is_true(enabled_in);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(calsci_keypad_set_press_printing_obj, calsci_keypad_set_press_printing);

static mp_obj_t calsci_keypad_press_printing(void) {
    return mp_obj_new_bool(calsci_keypad_print_presses);
}
static MP_DEFINE_CONST_FUN_OBJ_0(calsci_keypad_press_printing_obj, calsci_keypad_press_printing);

static const mp_rom_map_elem_t calsci_keypad_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_calsci_keypad) },
    { MP_ROM_QSTR(MP_QSTR_help), MP_ROM_PTR(&calsci_keypad_help_obj) },
    { MP_ROM_QSTR(MP_QSTR_Keypad), MP_ROM_PTR(&calsci_keypad_type) },
    { MP_ROM_QSTR(MP_QSTR_set_keypad_press_printing), MP_ROM_PTR(&calsci_keypad_set_press_printing_obj) },
    { MP_ROM_QSTR(MP_QSTR_keypad_press_printing), MP_ROM_PTR(&calsci_keypad_press_printing_obj) },
    { MP_ROM_QSTR(MP_QSTR_PULL_NONE), MP_ROM_INT(CALSCI_KEYPAD_PULL_NONE) },
    { MP_ROM_QSTR(MP_QSTR_PULL_UP), MP_ROM_INT(CALSCI_KEYPAD_PULL_UP) },
    { MP_ROM_QSTR(MP_QSTR_PULL_DOWN), MP_ROM_INT(CALSCI_KEYPAD_PULL_DOWN) },
};
static MP_DEFINE_CONST_DICT(calsci_keypad_module_globals, calsci_keypad_module_globals_table);

const mp_obj_module_t calsci_keypad_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&calsci_keypad_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_calsci_keypad, calsci_keypad_user_cmodule);
