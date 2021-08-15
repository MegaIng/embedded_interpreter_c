//
// Created by tramp on 2021-08-06.
//

#include "simple_errors.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <windows.h>

#define BUFFER_SIZE (2048-sizeof(long long))

static __thread struct {
    long long id;
    char message[BUFFER_SIZE];
} error_state = {0};

void set_error(long long id, const char *message) {
    error_state.id = id ? id : -1; // We don't allow error id 0 and silently replace it with -1
    strcpy_s(error_state.message, BUFFER_SIZE, message);
}

void set_errorf(long long id, const char *format, ...) {
    error_state.id = id ? id : -1; // We don't allow error id 0 and silently replace it with -1
    va_list args;
    va_start(args, format);
    vsprintf_s(error_state.message, BUFFER_SIZE, format, args);
    va_end(args);
}

void set_error_from_windows(const char *function_name) {
    error_state.id = GetLastError();
    size_t used = sprintf_s(error_state.message, BUFFER_SIZE, "%s failed with error code %llx:", function_name,
                            error_state.id);
    FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error_state.id,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            error_state.message + used - 1,
            BUFFER_SIZE - used + 1, NULL);

}


void unset_error(void) {
    error_state.id = 0;
}

bool has_error(void) {
    return error_state.id != 0;
}

long long get_error_id(void) {
    return error_state.id;
}

const char *get_error_message(void) {
    return error_state.message;
}

void error_exit(void) {
    if (error_state.id == 0) {
        fprintf(stderr, "error_exit called without error set");
        exit(EXIT_FAILURE);
    } else {
        fprintf(stderr, "error_exit with error id %llx:\n%.*s\n", error_state.id,
                (int) BUFFER_SIZE, error_state.message);
    }
    exit((int) error_state.id);
}
