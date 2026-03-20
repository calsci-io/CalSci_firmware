/*****************************************************************************
 * MicroPython C module: calsci_log
 *
 * Public Python-facing diagnostics API for the shared CalSci firmware log.
 ****************************************************************************/

#include "calsci_log.h"

#include <string.h>

#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"

typedef struct _calsci_log_help_entry_t {
    qstr name;
    const mp_obj_str_t *doc;
} calsci_log_help_entry_t;

static const MP_DEFINE_STR_OBJ(calsci_log_doc_module_obj,
    "Shared CalSci firmware log buffer.\n"
    "\n"
    "Use show([limit]) to inspect recent events, clear() to reset the\n"
    "buffer before a repro, and mark(level, subsystem, message) to add\n"
    "manual markers from Python while testing.\n"
    "\n"
    "After boot.py runs you can also use:\n"
    "  clogs()\n"
    "  clogs(10)\n"
    "  clog_clear()\n"
    "  clog_count()\n"
    "  clog_mark('INFO', 'test', 'ready')");

static const MP_DEFINE_STR_OBJ(calsci_log_doc_show_obj,
    "show([limit]) -> list of recent log dicts ordered oldest to newest.");
static const MP_DEFINE_STR_OBJ(calsci_log_doc_count_obj,
    "Return the current number of retained log entries.");
static const MP_DEFINE_STR_OBJ(calsci_log_doc_clear_obj,
    "Erase all retained log entries so the next repro starts clean.");
static const MP_DEFINE_STR_OBJ(calsci_log_doc_mark_obj,
    "mark(level, subsystem, message) -> append one log record from Python.");
static const MP_DEFINE_STR_OBJ(calsci_log_doc_capacity_obj,
    "Maximum number of log entries retained in the ring buffer.");

static const calsci_log_help_entry_t calsci_log_help_entries[] = {
    { MP_QSTR_show, &calsci_log_doc_show_obj },
    { MP_QSTR_count, &calsci_log_doc_count_obj },
    { MP_QSTR_clear, &calsci_log_doc_clear_obj },
    { MP_QSTR_mark, &calsci_log_doc_mark_obj },
    { MP_QSTR_CAPACITY, &calsci_log_doc_capacity_obj },
};

static void calsci_log_help_print_entry(const calsci_log_help_entry_t *entry) {
    mp_print_str(MP_PYTHON_PRINTER, "  ");
    mp_obj_print(MP_OBJ_NEW_QSTR(entry->name), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, " -- ");
    mp_obj_print(MP_OBJ_FROM_PTR(entry->doc), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
}

static void calsci_log_help_print_all(void) {
    mp_obj_print(MP_OBJ_FROM_PTR(&calsci_log_doc_module_obj), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
    for (size_t i = 0; i < MP_ARRAY_SIZE(calsci_log_help_entries); ++i) {
        calsci_log_help_print_entry(&calsci_log_help_entries[i]);
    }
}

static mp_obj_t mp_calsci_log_help(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        calsci_log_help_print_all();
        return mp_const_none;
    }

    qstr topic = mp_obj_str_get_qstr(args[0]);
    for (size_t i = 0; i < MP_ARRAY_SIZE(calsci_log_help_entries); ++i) {
        if (calsci_log_help_entries[i].name == topic) {
            mp_obj_print(MP_OBJ_FROM_PTR(calsci_log_help_entries[i].doc), PRINT_STR);
            mp_print_str(MP_PYTHON_PRINTER, "\n");
            return mp_const_none;
        }
    }

    mp_raise_ValueError(MP_ERROR_TEXT("unknown calsci_log topic"));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_calsci_log_help_obj, 0, 1, mp_calsci_log_help);

static mp_obj_t calsci_log_entries_from_limit(mp_int_t limit) {
    if (limit < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("limit must be >= 0"));
    }

    size_t max_entries = (size_t)limit;
    if (max_entries > CALSCI_LOG_CAPACITY) {
        max_entries = CALSCI_LOG_CAPACITY;
    }

    if (max_entries == 0) {
        return mp_obj_new_list(0, NULL);
    }

    calsci_log_entry_t entries[CALSCI_LOG_CAPACITY];
    size_t count = calsci_log_snapshot(entries, max_entries);
    mp_obj_t list = mp_obj_new_list(0, NULL);

    for (size_t i = 0; i < count; ++i) {
        mp_obj_t item = mp_obj_new_dict(0);
        mp_obj_dict_store(item, MP_OBJ_NEW_QSTR(MP_QSTR_seq), mp_obj_new_int_from_uint(entries[i].seq));
        mp_obj_dict_store(item, MP_OBJ_NEW_QSTR(MP_QSTR_level), mp_obj_new_str(entries[i].level, strlen(entries[i].level)));
        mp_obj_dict_store(item, MP_OBJ_NEW_QSTR(MP_QSTR_subsystem), mp_obj_new_str(entries[i].subsystem, strlen(entries[i].subsystem)));
        mp_obj_dict_store(item, MP_OBJ_NEW_QSTR(MP_QSTR_message), mp_obj_new_str(entries[i].message, strlen(entries[i].message)));
        mp_obj_list_append(list, item);
    }

    return list;
}

static mp_obj_t mp_calsci_log_show(size_t n_args, const mp_obj_t *args) {
    mp_int_t limit = (mp_int_t)calsci_log_count();
    if (n_args == 1) {
        limit = mp_obj_get_int(args[0]);
    }
    return calsci_log_entries_from_limit(limit);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_calsci_log_show_obj, 0, 1, mp_calsci_log_show);

static mp_obj_t mp_calsci_log_count(void) {
    return mp_obj_new_int_from_uint(calsci_log_count());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_calsci_log_count_obj, mp_calsci_log_count);

static mp_obj_t mp_calsci_log_clear(void) {
    calsci_log_clear();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_calsci_log_clear_obj, mp_calsci_log_clear);

static mp_obj_t mp_calsci_log_mark(size_t n_args, const mp_obj_t *args) {
    const char *level = mp_obj_str_get_str(args[0]);
    const char *subsystem = mp_obj_str_get_str(args[1]);
    const char *message = mp_obj_str_get_str(args[2]);
    calsci_log_write(level, subsystem, message);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_calsci_log_mark_obj, 3, 3, mp_calsci_log_mark);

static const mp_rom_map_elem_t calsci_log_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_calsci_log) },
    { MP_ROM_QSTR(MP_QSTR_help), MP_ROM_PTR(&mp_calsci_log_help_obj) },
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&mp_calsci_log_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_count), MP_ROM_PTR(&mp_calsci_log_count_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&mp_calsci_log_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_mark), MP_ROM_PTR(&mp_calsci_log_mark_obj) },
    { MP_ROM_QSTR(MP_QSTR_CAPACITY), MP_ROM_INT(CALSCI_LOG_CAPACITY) },
};
static MP_DEFINE_CONST_DICT(calsci_log_module_globals, calsci_log_module_globals_table);

const mp_obj_module_t calsci_log_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&calsci_log_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_calsci_log, calsci_log_user_cmodule);
