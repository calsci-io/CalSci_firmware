#include <stdbool.h>
#include <string.h>

#include "calsci_runtime.h"

#include "py/compile.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/builtin.h"
#include "py/mpstate.h"
#include "py/mpthread.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"

#include "extmod/vfs.h"

#include "shared/runtime/pyexec.h"

#include "ports/esp32/mpthreadport.h"

#include "esp_task.h"

#define CALSCI_MAIN_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 1)
#define CALSCI_MAIN_FILENAME_MAX_LEN (64)

extern const mp_obj_module_t calsci_help_user_cmodule;
extern const mp_obj_module_t calsci_log_user_cmodule;
extern const mp_obj_module_t calsci_runtime_user_cmodule;

static volatile bool calsci_keypad_blocked = false;

typedef struct _calsci_main_thread_args_t {
    size_t stack_size;
    char filename[CALSCI_MAIN_FILENAME_MAX_LEN];
} calsci_main_thread_args_t;

static void calsci_register_builtin_name(qstr name, mp_obj_t obj) {
    #if MICROPY_CAN_OVERRIDE_BUILTINS
    if (MP_STATE_VM(mp_module_builtins_override_dict) == NULL) {
        MP_STATE_VM(mp_module_builtins_override_dict) = MP_OBJ_TO_PTR(mp_obj_new_dict(1));
    }
    mp_obj_dict_store(
        MP_OBJ_FROM_PTR(MP_STATE_VM(mp_module_builtins_override_dict)),
        MP_OBJ_NEW_QSTR(name),
        obj
    );
    #else
    mp_store_global(name, obj);
    #endif
}

static mp_obj_t calsci_module_attr(const mp_obj_module_t *module, qstr name) {
    mp_map_elem_t *elem = mp_map_lookup(
        &module->globals->map,
        MP_OBJ_NEW_QSTR(name),
        MP_MAP_LOOKUP
    );
    if (elem == NULL) {
        return mp_const_none;
    }
    return elem->value;
}

bool calsci_runtime_set_keypad_blocked(bool blocked) {
    calsci_keypad_blocked = blocked;
    return blocked;
}

bool calsci_runtime_keypad_blocked(void) {
    return calsci_keypad_blocked;
}

void calsci_runtime_wait_if_keypad_blocked(void (*release_cb)(void *), void *ctx) {
    while (calsci_runtime_keypad_blocked()) {
        if (release_cb != NULL) {
            release_cb(ctx);
        }
        mp_hal_delay_ms(CALSCI_RUNTIME_WAIT_SLICE_MS);
    }
}

static const MP_DEFINE_STR_OBJ(calsci_runtime_doc_module_obj,
    "CalSci runtime helpers.\n"
    "\n"
    "This module keeps keypad ownership state in firmware and starts main.py\n"
    "on a background MicroPython thread without holding REPL interrupt state.");
static const MP_DEFINE_STR_OBJ(calsci_runtime_doc_help_obj,
    "Print CalSci runtime helper documentation.");
static const MP_DEFINE_STR_OBJ(calsci_runtime_doc_set_blocked_obj,
    "set_calsci_keypad_blocked(blocked) -> set the keypad ownership flag.");
static const MP_DEFINE_STR_OBJ(calsci_runtime_doc_block_obj,
    "block_calsci_keypad() -> mark the keypad as owned by host/REPL.");
static const MP_DEFINE_STR_OBJ(calsci_runtime_doc_unblock_obj,
    "unblock_calsci_keypad() -> return keypad ownership to the device.");
static const MP_DEFINE_STR_OBJ(calsci_runtime_doc_is_blocked_obj,
    "calsci_keypad_blocked() -> read the keypad ownership flag.");
static const MP_DEFINE_STR_OBJ(calsci_runtime_doc_wait_obj,
    "wait_if_repl_busy(cb=None) -> wait until keypad ownership returns; cb() is retried while blocked.");

typedef struct _calsci_runtime_help_entry_t {
    qstr name;
    const mp_obj_str_t *doc;
} calsci_runtime_help_entry_t;

static const calsci_runtime_help_entry_t calsci_runtime_help_entries[] = {
    { MP_QSTR_help, &calsci_runtime_doc_help_obj },
    { MP_QSTR_set_calsci_keypad_blocked, &calsci_runtime_doc_set_blocked_obj },
    { MP_QSTR_block_calsci_keypad, &calsci_runtime_doc_block_obj },
    { MP_QSTR_unblock_calsci_keypad, &calsci_runtime_doc_unblock_obj },
    { MP_QSTR_calsci_keypad_blocked, &calsci_runtime_doc_is_blocked_obj },
    { MP_QSTR_wait_if_repl_busy, &calsci_runtime_doc_wait_obj },
};

static void calsci_runtime_help_print_entry(const calsci_runtime_help_entry_t *entry) {
    mp_print_str(MP_PYTHON_PRINTER, "  ");
    mp_obj_print(MP_OBJ_NEW_QSTR(entry->name), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, " -- ");
    mp_obj_print(MP_OBJ_FROM_PTR(entry->doc), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
}

static void calsci_runtime_help_print_all(void) {
    mp_obj_print(MP_OBJ_FROM_PTR(&calsci_runtime_doc_module_obj), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
    for (size_t i = 0; i < MP_ARRAY_SIZE(calsci_runtime_help_entries); ++i) {
        calsci_runtime_help_print_entry(&calsci_runtime_help_entries[i]);
    }
}

static mp_obj_t mp_calsci_runtime_help(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        calsci_runtime_help_print_all();
        return mp_const_none;
    }

    qstr topic = mp_obj_str_get_qstr(args[0]);
    for (size_t i = 0; i < MP_ARRAY_SIZE(calsci_runtime_help_entries); ++i) {
        if (calsci_runtime_help_entries[i].name == topic) {
            mp_obj_print(MP_OBJ_FROM_PTR(calsci_runtime_help_entries[i].doc), PRINT_STR);
            mp_print_str(MP_PYTHON_PRINTER, "\n");
            return mp_const_none;
        }
    }

    mp_raise_ValueError(MP_ERROR_TEXT("unknown calsci_runtime help topic"));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_calsci_runtime_help_obj, 0, 1, mp_calsci_runtime_help);

static mp_obj_t mp_calsci_set_keypad_blocked(mp_obj_t blocked_in) {
    return mp_obj_new_bool(calsci_runtime_set_keypad_blocked(mp_obj_is_true(blocked_in)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_calsci_set_keypad_blocked_obj, mp_calsci_set_keypad_blocked);

static mp_obj_t mp_calsci_block_keypad(void) {
    return mp_obj_new_bool(calsci_runtime_set_keypad_blocked(true));
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_calsci_block_keypad_obj, mp_calsci_block_keypad);

static mp_obj_t mp_calsci_unblock_keypad(void) {
    return mp_obj_new_bool(calsci_runtime_set_keypad_blocked(false));
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_calsci_unblock_keypad_obj, mp_calsci_unblock_keypad);

static mp_obj_t mp_calsci_keypad_is_blocked(void) {
    return mp_obj_new_bool(calsci_runtime_keypad_blocked());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_calsci_keypad_is_blocked_obj, mp_calsci_keypad_is_blocked);

static mp_obj_t mp_calsci_wait_if_repl_busy(size_t n_args, const mp_obj_t *args) {
    mp_obj_t release_cb = mp_const_none;
    if (n_args == 1) {
        release_cb = args[0];
        if (release_cb != mp_const_none && !mp_obj_is_callable(release_cb)) {
            mp_raise_TypeError(MP_ERROR_TEXT("cb must be callable"));
        }
    }

    while (calsci_runtime_keypad_blocked()) {
        if (release_cb != mp_const_none) {
            nlr_buf_t nlr;
            nlr.ret_val = NULL;
            if (nlr_push(&nlr) == 0) {
                mp_call_function_0(release_cb);
                nlr_pop();
            } else {
                mp_handle_pending(MP_HANDLE_PENDING_CALLBACKS_AND_CLEAR_EXCEPTIONS);
            }
        }
        mp_hal_delay_ms(CALSCI_RUNTIME_WAIT_SLICE_MS);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_calsci_wait_if_repl_busy_obj, 0, 1, mp_calsci_wait_if_repl_busy);

static const mp_rom_map_elem_t calsci_runtime_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_calsci_runtime) },
    { MP_ROM_QSTR(MP_QSTR_help), MP_ROM_PTR(&mp_calsci_runtime_help_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_calsci_keypad_blocked), MP_ROM_PTR(&mp_calsci_set_keypad_blocked_obj) },
    { MP_ROM_QSTR(MP_QSTR_block_calsci_keypad), MP_ROM_PTR(&mp_calsci_block_keypad_obj) },
    { MP_ROM_QSTR(MP_QSTR_unblock_calsci_keypad), MP_ROM_PTR(&mp_calsci_unblock_keypad_obj) },
    { MP_ROM_QSTR(MP_QSTR_calsci_keypad_blocked), MP_ROM_PTR(&mp_calsci_keypad_is_blocked_obj) },
    { MP_ROM_QSTR(MP_QSTR_wait_if_repl_busy), MP_ROM_PTR(&mp_calsci_wait_if_repl_busy_obj) },
};
static MP_DEFINE_CONST_DICT(calsci_runtime_module_globals, calsci_runtime_module_globals_table);

const mp_obj_module_t calsci_runtime_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&calsci_runtime_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_calsci_runtime, calsci_runtime_user_cmodule);

static int calsci_exec_file_no_interrupt(const char *filename) {
    #if MICROPY_ENABLE_COMPILER
    int ret = PYEXEC_NORMAL_EXIT;
    nlr_buf_t nlr;
    nlr.ret_val = NULL;

    if (nlr_push(&nlr) == 0) {
        mp_lexer_t *lex = mp_lexer_new_from_file(qstr_from_str(filename));
        qstr source_name = lex->source_name;
        #if MICROPY_MODULE___FILE__
        mp_store_global(MP_QSTR___file__, MP_OBJ_NEW_QSTR(source_name));
        #endif
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, false);
        mp_call_function_0(module_fun);
        mp_handle_pending(MP_HANDLE_PENDING_CALLBACKS_AND_EXCEPTIONS);
        nlr_pop();
        return ret;
    }

    mp_handle_pending(MP_HANDLE_PENDING_CALLBACKS_AND_CLEAR_EXCEPTIONS);
    mp_obj_t exc_obj = MP_OBJ_FROM_PTR(nlr.ret_val);
    if (mp_obj_is_subclass_fast(
        MP_OBJ_FROM_PTR(((mp_obj_base_t *)nlr.ret_val)->type),
        MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
        #if MICROPY_PYEXEC_ENABLE_EXIT_CODE_HANDLING
        mp_obj_t val = mp_obj_exception_get_value(exc_obj);
        if (val != mp_const_none) {
            if (mp_obj_is_int(val)) {
                ret = (int)mp_obj_int_get_truncated(val);
            } else {
                mp_obj_print_helper(MICROPY_ERROR_PRINTER, val, PRINT_STR);
                mp_print_str(MICROPY_ERROR_PRINTER, "\n");
                ret = PYEXEC_UNHANDLED_EXCEPTION;
            }
        }
        #endif
        return ret | PYEXEC_FORCED_EXIT;
    }

    mp_obj_print_exception(&mp_plat_print, exc_obj);
    ret = PYEXEC_UNHANDLED_EXCEPTION;
    #if MICROPY_PYEXEC_ENABLE_EXIT_CODE_HANDLING
    if (mp_obj_is_subclass_fast(
        MP_OBJ_FROM_PTR(((mp_obj_base_t *)nlr.ret_val)->type),
        MP_OBJ_FROM_PTR(&mp_type_KeyboardInterrupt))) {
        ret = PYEXEC_KEYBOARD_INTERRUPT;
    }
    #endif
    return ret;
    #else
    (void)filename;
    mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("script compilation not supported"));
    return PYEXEC_UNHANDLED_EXCEPTION;
    #endif
}

static void *calsci_main_thread(void *arg_in) {
    calsci_main_thread_args_t *args = (calsci_main_thread_args_t *)arg_in;

    mp_state_thread_t ts;
    mp_thread_init_state(&ts, args->stack_size, NULL, NULL);

    #if MICROPY_ENABLE_PYSTACK
    mp_obj_t mini_pystack[128];
    mp_pystack_init(mini_pystack, &mini_pystack[128]);
    #endif

    MP_THREAD_GIL_ENTER();
    mp_thread_start();
    calsci_exec_file_no_interrupt(args->filename);
    mp_thread_finish();
    MP_THREAD_GIL_EXIT();

    return NULL;
}

void calsci_port_init(void) {
    #if MICROPY_PY_SYS_PS1_PS2
    MP_STATE_VM(sys_mutable[MP_SYS_MUTABLE_PS1]) = mp_obj_new_str("CalSci >>> ", 11);
    MP_STATE_VM(sys_mutable[MP_SYS_MUTABLE_PS2]) = mp_obj_new_str("... ", 4);
    #endif

    calsci_register_builtin_name(MP_QSTR_calsci_help, MP_OBJ_FROM_PTR(&calsci_help_user_cmodule));
    calsci_register_builtin_name(MP_QSTR_calsci_log, MP_OBJ_FROM_PTR(&calsci_log_user_cmodule));
    calsci_register_builtin_name(MP_QSTR_chelp, calsci_module_attr(&calsci_help_user_cmodule, MP_QSTR_show));
    calsci_register_builtin_name(MP_QSTR_ctopics, calsci_module_attr(&calsci_help_user_cmodule, MP_QSTR_topics));
    calsci_register_builtin_name(MP_QSTR_clogs, calsci_module_attr(&calsci_log_user_cmodule, MP_QSTR_show));
    calsci_register_builtin_name(MP_QSTR_clog_clear, calsci_module_attr(&calsci_log_user_cmodule, MP_QSTR_clear));
    calsci_register_builtin_name(MP_QSTR_clog_count, calsci_module_attr(&calsci_log_user_cmodule, MP_QSTR_count));
    calsci_register_builtin_name(MP_QSTR_clog_mark, calsci_module_attr(&calsci_log_user_cmodule, MP_QSTR_mark));
    calsci_register_builtin_name(MP_QSTR_set_calsci_keypad_blocked, calsci_module_attr(&calsci_runtime_user_cmodule, MP_QSTR_set_calsci_keypad_blocked));
    calsci_register_builtin_name(MP_QSTR_block_calsci_keypad, calsci_module_attr(&calsci_runtime_user_cmodule, MP_QSTR_block_calsci_keypad));
    calsci_register_builtin_name(MP_QSTR_unblock_calsci_keypad, calsci_module_attr(&calsci_runtime_user_cmodule, MP_QSTR_unblock_calsci_keypad));
    calsci_register_builtin_name(MP_QSTR_calsci_keypad_blocked, calsci_module_attr(&calsci_runtime_user_cmodule, MP_QSTR_calsci_keypad_blocked));
    calsci_register_builtin_name(MP_QSTR_wait_if_repl_busy, calsci_module_attr(&calsci_runtime_user_cmodule, MP_QSTR_wait_if_repl_busy));
}

int calsci_run_main_file_if_exists(const char *filename) {
    if (mp_import_stat(filename) != MP_IMPORT_STAT_FILE) {
        return 1;
    }

    calsci_main_thread_args_t *args = m_new_obj(calsci_main_thread_args_t);
    memset(args, 0, sizeof(*args));
    args->stack_size = CALSCI_MAIN_TASK_STACK_SIZE;
    strncpy(args->filename, filename, sizeof(args->filename) - 1);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_thread_create_ex(
            calsci_main_thread,
            args,
            &args->stack_size,
            CALSCI_MAIN_TASK_PRIORITY,
            "CalSciMain");
        nlr_pop();
        return 1;
    }

    mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
    return PYEXEC_UNHANDLED_EXCEPTION;
}
