#include <string.h>

#include "calsci_runtime.h"

#include "py/obj.h"
#include "py/mpstate.h"
#include "py/mpthread.h"
#include "py/nlr.h"
#include "py/runtime.h"

#include "extmod/vfs.h"

#include "shared/runtime/pyexec.h"

#include "ports/esp32/mpthreadport.h"

#include "esp_task.h"

#define CALSCI_MAIN_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 1)
#define CALSCI_MAIN_FILENAME_MAX_LEN (64)

typedef struct _calsci_main_thread_args_t {
    size_t stack_size;
    char filename[CALSCI_MAIN_FILENAME_MAX_LEN];
} calsci_main_thread_args_t;

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
    pyexec_file(args->filename);
    mp_thread_finish();
    MP_THREAD_GIL_EXIT();

    return NULL;
}

void calsci_port_init(void) {
    #if MICROPY_PY_SYS_PS1_PS2
    MP_STATE_VM(sys_mutable[MP_SYS_MUTABLE_PS1]) = mp_obj_new_str("CalSci >>> ", 11);
    MP_STATE_VM(sys_mutable[MP_SYS_MUTABLE_PS2]) = mp_obj_new_str("... ", 4);
    #endif
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
