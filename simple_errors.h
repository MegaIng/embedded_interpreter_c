//
// Created by tramp on 2021-08-06.
//

#ifndef EMBEDDED_INTERPRETER_SIMPLE_ERRORS_H
#define EMBEDDED_INTERPRETER_SIMPLE_ERRORS_H

#include <stdbool.h>


void set_error(long long id, const char *message);

void set_errorf(long long id, const char *format, ...) __attribute__ ((format(printf, 2, 3)));

void set_error_from_windows(const char*function_name);

void unset_error(void);

bool has_error(void);

long long get_error_id(void);

const char *get_error_message(void);

void error_exit(void) __attribute__ ((noreturn));


#endif //EMBEDDED_INTERPRETER_SIMPLE_ERRORS_H
