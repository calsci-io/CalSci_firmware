#ifndef CALSCI_LOG_H
#define CALSCI_LOG_H

#include <stddef.h>
#include <stdint.h>

#define CALSCI_LOG_CAPACITY (32)
#define CALSCI_LOG_LEVEL_MAX_LEN (8)
#define CALSCI_LOG_SUBSYSTEM_MAX_LEN (24)
#define CALSCI_LOG_MESSAGE_MAX_LEN (96)

typedef struct _calsci_log_entry_t {
    uint32_t seq;
    char level[CALSCI_LOG_LEVEL_MAX_LEN];
    char subsystem[CALSCI_LOG_SUBSYSTEM_MAX_LEN];
    char message[CALSCI_LOG_MESSAGE_MAX_LEN];
} calsci_log_entry_t;

void calsci_log_write(const char *level, const char *subsystem, const char *msg);
void calsci_log_writef(const char *level, const char *subsystem, const char *fmt, ...);
void calsci_log_clear(void);
size_t calsci_log_count(void);
size_t calsci_log_snapshot(calsci_log_entry_t *entries, size_t max_entries);

#define CALSCI_LOG_INFO(subsystem, msg) calsci_log_write("INFO", subsystem, msg)
#define CALSCI_LOG_WARN(subsystem, msg) calsci_log_write("WARN", subsystem, msg)
#define CALSCI_LOG_ERROR(subsystem, msg) calsci_log_write("ERROR", subsystem, msg)

#endif // CALSCI_LOG_H
