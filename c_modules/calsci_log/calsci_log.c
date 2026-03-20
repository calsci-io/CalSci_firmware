#include "calsci_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct _calsci_log_state_t {
    calsci_log_entry_t entries[CALSCI_LOG_CAPACITY];
    size_t count;
    size_t head;
    uint32_t next_seq;
} calsci_log_state_t;

static calsci_log_state_t s_calsci_log_state;

static const char *calsci_log_default_string(const char *value, const char *fallback) {
    if (value != NULL && value[0] != '\0') {
        return value;
    }
    return fallback;
}

static void calsci_log_copy_field(char *dst, size_t dst_len, const char *src, const char *fallback) {
    if (dst_len == 0) {
        return;
    }

    const char *value = calsci_log_default_string(src, fallback);
    strncpy(dst, value, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

void calsci_log_write(const char *level, const char *subsystem, const char *msg) {
    calsci_log_entry_t *entry = &s_calsci_log_state.entries[s_calsci_log_state.head];

    entry->seq = ++s_calsci_log_state.next_seq;
    calsci_log_copy_field(entry->level, sizeof(entry->level), level, "INFO");
    calsci_log_copy_field(entry->subsystem, sizeof(entry->subsystem), subsystem, "core");
    calsci_log_copy_field(entry->message, sizeof(entry->message), msg, "");

    s_calsci_log_state.head = (s_calsci_log_state.head + 1) % CALSCI_LOG_CAPACITY;
    if (s_calsci_log_state.count < CALSCI_LOG_CAPACITY) {
        ++s_calsci_log_state.count;
    }
}

void calsci_log_writef(const char *level, const char *subsystem, const char *fmt, ...) {
    char message[CALSCI_LOG_MESSAGE_MAX_LEN];

    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), calsci_log_default_string(fmt, ""), args);
    va_end(args);

    message[sizeof(message) - 1] = '\0';
    calsci_log_write(level, subsystem, message);
}

void calsci_log_clear(void) {
    memset(s_calsci_log_state.entries, 0, sizeof(s_calsci_log_state.entries));
    s_calsci_log_state.count = 0;
    s_calsci_log_state.head = 0;
}

size_t calsci_log_count(void) {
    return s_calsci_log_state.count;
}

size_t calsci_log_snapshot(calsci_log_entry_t *entries, size_t max_entries) {
    if (entries == NULL || max_entries == 0) {
        return 0;
    }

    size_t available = s_calsci_log_state.count;
    size_t count = available < max_entries ? available : max_entries;
    size_t start = (s_calsci_log_state.head + CALSCI_LOG_CAPACITY - s_calsci_log_state.count) % CALSCI_LOG_CAPACITY;

    if (available > count) {
        start = (start + (available - count)) % CALSCI_LOG_CAPACITY;
    }

    for (size_t i = 0; i < count; ++i) {
        entries[i] = s_calsci_log_state.entries[(start + i) % CALSCI_LOG_CAPACITY];
    }

    return count;
}
