#ifndef CALSCI_RUNTIME_H
#define CALSCI_RUNTIME_H

#include <stddef.h>

// Strict device identity used by the VS Code extension.
#ifndef MICROPY_HW_USB_PRODUCT_FS_STRING
#define MICROPY_HW_USB_PRODUCT_FS_STRING "CalSci"
#endif

#ifndef MICROPY_HW_USB_MANUFACTURER_STRING
#define MICROPY_HW_USB_MANUFACTURER_STRING "CalSci"
#endif

#ifndef CALSCI_MAIN_TASK_STACK_SIZE
#define CALSCI_MAIN_TASK_STACK_SIZE (32 * 1024)
#endif

void calsci_port_init(void);
int calsci_run_main_file_if_exists(const char *filename);

#ifndef MICROPY_PORT_INIT_FUNC
#define MICROPY_PORT_INIT_FUNC calsci_port_init()
#endif

#endif // CALSCI_RUNTIME_H
