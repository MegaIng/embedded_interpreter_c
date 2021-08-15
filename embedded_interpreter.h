#ifndef EMBEDDED_INTERPRETER_EMBEDDED_INTERPRETER_H
#define EMBEDDED_INTERPRETER_EMBEDDED_INTERPRETER_H

typedef struct Bytecode Bytecode;
typedef struct Interpreter Interpreter;

void setup(void);

void bytecode_delete(Bytecode *bytecode);

Bytecode *bytecode_from_json(char *string);

Interpreter *interpreter_for_bytecode(Bytecode *bytecode);

void interpreter_run(Interpreter *interpreter);

void interpreter_delete(Interpreter *interpreter);

#endif //EMBEDDED_INTERPRETER_EMBEDDED_INTERPRETER_H
