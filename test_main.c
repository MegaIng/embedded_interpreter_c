//
// Created by MegaIng on 2021-08-05.
//
#include <stdlib.h>
#include <stdio.h>
#include "embedded_interpreter.h"
#include "simple_errors.h"


int main(int argc, char *argv[]) {
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    char buffer[1024] = {0};
    char *file_name = argv[1];
    FILE *f = fopen(file_name, "r");
    fread(buffer, sizeof(char), 1024, f);
    setup();
    Bytecode *bytecode = bytecode_from_json(buffer);
    if (has_error()) {
        error_exit();
    }
    Interpreter *interpreter = interpreter_for_bytecode(bytecode);
    interpreter_run(interpreter);
    if (has_error()) {
        error_exit();
    }
    bytecode_delete(bytecode);
    return EXIT_SUCCESS;
}