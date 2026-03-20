/*****************************************************************************
 * MicroPython C module: calsci_help
 *
 * Central help index for CalSci-specific modules and boot-time helper APIs.
 ****************************************************************************/

#include "py/obj.h"
#include "py/objlist.h"
#include "py/objstr.h"
#include "py/runtime.h"

typedef struct _calsci_help_entry_t {
    qstr name;
    const mp_obj_str_t *summary;
    const mp_obj_str_t *doc;
} calsci_help_entry_t;

static const MP_DEFINE_STR_OBJ(calsci_help_doc_module_obj,
    "CalSci help index.\n"
    "\n"
    "Use calsci_help.show() to list CalSci-specific topics, or\n"
    "calsci_help.show(topic) to print one topic in full.\n"
    "\n"
    "After boot.py runs you can use the short builtins:\n"
    "  chelp()\n"
    "  chelp('display')\n"
    "  ctopics()");

static const MP_DEFINE_STR_OBJ(calsci_help_summary_display_obj,
    "ST7565 display driver help entry; also available as display.help().");
static const MP_DEFINE_STR_OBJ(calsci_help_summary_st7565_obj,
    "Alias of the display topic for the ST7565 driver module.");
static const MP_DEFINE_STR_OBJ(calsci_help_summary_hybrid_sim_obj,
    "Framebuffer capture module used by the hybrid bridge.");
static const MP_DEFINE_STR_OBJ(calsci_help_summary_tools_obj,
    "Partial-refresh helper module for display.graphics wrappers.");
static const MP_DEFINE_STR_OBJ(calsci_help_summary_hybrid_obj,
    "boot.py hybrid bridge mode helpers and debounce controls.");
static const MP_DEFINE_STR_OBJ(calsci_help_summary_runtime_obj,
    "boot.py keypad ownership and REPL wait helpers.");
static const MP_DEFINE_STR_OBJ(calsci_help_summary_log_obj,
    "Shared internal firmware log module.");

static const MP_DEFINE_STR_OBJ(calsci_help_doc_display_obj,
    "Display driver help lives in the st7565 module.\n"
    "\n"
    "Use:\n"
    "  display.help()\n"
    "\n"
    "For one symbol only:\n"
    "  display.help('graphics')\n"
    "  display.help('set_contrast')");

static const MP_DEFINE_STR_OBJ(calsci_help_doc_hybrid_sim_obj,
    "Framebuffer capture help lives in the hybrid_sim module.\n"
    "\n"
    "Use:\n"
    "  import hybrid_sim\n"
    "  hybrid_sim.help()\n"
    "\n"
    "For one symbol only:\n"
    "  hybrid_sim.help('status')\n"
    "  hybrid_sim.help('poll_state')");

static const MP_DEFINE_STR_OBJ(calsci_help_doc_tools_obj,
    "Partial-refresh helper help lives in the tools module.\n"
    "\n"
    "Use:\n"
    "  import tools\n"
    "  tools.help()\n"
    "\n"
    "The wrapper returned by tools.refresh(...) also exposes help():\n"
    "  wrapper = tools.refresh(display.graphics)\n"
    "  wrapper.help()\n"
    "  wrapper.help('pixels_changed')");

static const MP_DEFINE_STR_OBJ(calsci_help_doc_hybrid_obj,
    "Hybrid helpers exported by boot.py:\n"
    "  hyb_enter_local_mode()                  -- disable hybrid capture and keep keypad local.\n"
    "  hyb_enter_command_mode()                -- command bridge mode with hybrid capture off.\n"
    "  hyb_enter_exec_mode()                   -- exec/raw-terminal mode with hybrid capture off.\n"
    "  hyb_enter_hybrid_mode(stream_enabled=0) -- enable hybrid capture and optionally sync a full frame.\n"
    "  hyb_stream_set_enabled(enabled)         -- enable or disable bridge streaming.\n"
    "  hyb_stream_is_enabled()                 -- return current stream-enabled flag.\n"
    "  hyb_bridge_status()                     -- return bridge state and debounce info.\n"
    "  hyb_delay_set_global(sec)               -- set default keypad debounce.\n"
    "  hyb_delay_set_local(name, sec)          -- set a named local debounce, e.g. graph.\n"
    "  hyb_delay_use_global()                  -- apply the global debounce now.\n"
    "  hyb_delay_use_local(name)               -- apply a named local debounce now.\n"
    "  hyb_keypad_input()                      -- bridge keypad reader used while hybrid mode owns input.\n"
    "  hyb_stream_updated_buffer()             -- bridge wait loop for streamed framebuffer updates.");

static const MP_DEFINE_STR_OBJ(calsci_help_doc_runtime_obj,
    "Runtime helpers exported by boot.py:\n"
    "  set_calsci_keypad_blocked(blocked) -- set keypad ownership flag explicitly.\n"
    "  block_calsci_keypad()              -- mark the keypad as owned by host/REPL.\n"
    "  unblock_calsci_keypad()            -- return keypad ownership to local scanning.\n"
    "  calsci_keypad_blocked()            -- read the keypad ownership flag.\n"
    "  wait_if_repl_busy(cb=None)         -- wait until the keypad block is released.");
static const MP_DEFINE_STR_OBJ(calsci_help_doc_log_obj,
    "Firmware logging is exposed by the calsci_log module.\n"
    "\n"
    "Use:\n"
    "  calsci_log.show()\n"
    "  calsci_log.show(10)\n"
    "  calsci_log.clear()\n"
    "  calsci_log.count()\n"
    "  calsci_log.mark('INFO', 'test', 'ready')\n"
    "\n"
    "After boot.py runs you can use:\n"
    "  clogs()\n"
    "  clogs(10)\n"
    "  clog_clear()\n"
    "  clog_count()\n"
    "  clog_mark('INFO', 'test', 'ready')");

static const calsci_help_entry_t calsci_help_entries[] = {
    { MP_QSTR_display, &calsci_help_summary_display_obj, &calsci_help_doc_display_obj },
    { MP_QSTR_st7565, &calsci_help_summary_st7565_obj, &calsci_help_doc_display_obj },
    { MP_QSTR_hybrid_sim, &calsci_help_summary_hybrid_sim_obj, &calsci_help_doc_hybrid_sim_obj },
    { MP_QSTR_tools, &calsci_help_summary_tools_obj, &calsci_help_doc_tools_obj },
    { MP_QSTR_hybrid, &calsci_help_summary_hybrid_obj, &calsci_help_doc_hybrid_obj },
    { MP_QSTR_runtime, &calsci_help_summary_runtime_obj, &calsci_help_doc_runtime_obj },
    { MP_QSTR_log, &calsci_help_summary_log_obj, &calsci_help_doc_log_obj },
};

static void calsci_help_print_summary(const calsci_help_entry_t *entry) {
    mp_print_str(MP_PYTHON_PRINTER, "  ");
    mp_obj_print(MP_OBJ_NEW_QSTR(entry->name), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, " -- ");
    mp_obj_print(MP_OBJ_FROM_PTR(entry->summary), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
}

static void calsci_help_print_all(void) {
    mp_obj_print(MP_OBJ_FROM_PTR(&calsci_help_doc_module_obj), PRINT_STR);
    mp_print_str(MP_PYTHON_PRINTER, "\n");
    for (size_t i = 0; i < MP_ARRAY_SIZE(calsci_help_entries); ++i) {
        calsci_help_print_summary(&calsci_help_entries[i]);
    }
}

static mp_obj_t mp_calsci_help_show(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        calsci_help_print_all();
        return mp_const_none;
    }

    qstr topic = mp_obj_str_get_qstr(args[0]);
    for (size_t i = 0; i < MP_ARRAY_SIZE(calsci_help_entries); ++i) {
        if (calsci_help_entries[i].name == topic) {
            mp_obj_print(MP_OBJ_FROM_PTR(calsci_help_entries[i].doc), PRINT_STR);
            mp_print_str(MP_PYTHON_PRINTER, "\n");
            return mp_const_none;
        }
    }

    mp_raise_ValueError(MP_ERROR_TEXT("unknown calsci_help topic"));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_calsci_help_show_obj, 0, 1, mp_calsci_help_show);

static mp_obj_t mp_calsci_help_topics(void) {
    mp_obj_t list = mp_obj_new_list(0, NULL);
    for (size_t i = 0; i < MP_ARRAY_SIZE(calsci_help_entries); ++i) {
        mp_obj_list_append(list, MP_OBJ_NEW_QSTR(calsci_help_entries[i].name));
    }
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_calsci_help_topics_obj, mp_calsci_help_topics);

static const mp_rom_map_elem_t calsci_help_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_calsci_help) },
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&mp_calsci_help_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_topics), MP_ROM_PTR(&mp_calsci_help_topics_obj) },
    { MP_ROM_QSTR(MP_QSTR_display), MP_ROM_QSTR(MP_QSTR_display) },
    { MP_ROM_QSTR(MP_QSTR_st7565), MP_ROM_QSTR(MP_QSTR_st7565) },
    { MP_ROM_QSTR(MP_QSTR_hybrid_sim), MP_ROM_QSTR(MP_QSTR_hybrid_sim) },
    { MP_ROM_QSTR(MP_QSTR_tools), MP_ROM_QSTR(MP_QSTR_tools) },
    { MP_ROM_QSTR(MP_QSTR_hybrid), MP_ROM_QSTR(MP_QSTR_hybrid) },
    { MP_ROM_QSTR(MP_QSTR_runtime), MP_ROM_QSTR(MP_QSTR_runtime) },
};
static MP_DEFINE_CONST_DICT(calsci_help_module_globals, calsci_help_module_globals_table);

const mp_obj_module_t calsci_help_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&calsci_help_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_calsci_help, calsci_help_user_cmodule);
