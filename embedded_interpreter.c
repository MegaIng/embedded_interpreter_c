#include "embedded_interpreter.h"
#include "cJSON/cJSON.h"
#include "simple_errors.h"

#include <windows.h>
#include <stdio.h>
#include <ffi.h>
#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>

#define fatal(message, ...) do { \
fprintf(stderr, "Fatal [%s:%i] " message "\n", __FUNCTION__ , __LINE__ ,##__VA_ARGS__); \
    abort(); \
} while (0)

typedef void (*Function)(void);

Function load_function(const char *dll_name, const char *function_name) {
    if (strcmp(dll_name, "%crt%") == 0) {
        dll_name = "msvcrt";
    }
    HMODULE dll = LoadLibraryA(dll_name);
    if (dll == NULL) {
        set_error_from_windows("LoadLibraryA");
        return NULL;
    }
    return (Function) GetProcAddress(dll, function_name);
}


typedef union Register {
    void *ptr;
    int8_t value_sint8;
    int16_t value_sint16;
    int32_t value_sint32;
    int64_t value_sint64;

    uint8_t value_uint8;
    uint16_t value_uint16;
    uint32_t value_uint32;
    uint16_t value_uint64;

    float value_float;
    double value_double;
} Register;


C_ASSERT(sizeof(void *) == sizeof(size_t));

C_ASSERT(sizeof(Register) == sizeof(size_t));

C_ASSERT(sizeof(&setup) == sizeof(size_t));


ffi_type *get_ffi_type(int id) {
    switch (id) {
        case FFI_TYPE_VOID:
            return &ffi_type_void;
        case FFI_TYPE_FLOAT:
            return &ffi_type_float;
        case FFI_TYPE_DOUBLE:
            return &ffi_type_double;
        case FFI_TYPE_UINT8:
            return &ffi_type_uint8;
        case FFI_TYPE_SINT8:
            return &ffi_type_sint8;
        case FFI_TYPE_UINT16:
            return &ffi_type_uint16;
        case FFI_TYPE_SINT16:
            return &ffi_type_sint16;
        case FFI_TYPE_UINT32:
            return &ffi_type_uint32;
        case FFI_TYPE_SINT32:
            return &ffi_type_sint32;
        case FFI_TYPE_UINT64:
            return &ffi_type_uint64;
        case FFI_TYPE_SINT64:
            return &ffi_type_sint64;
        case FFI_TYPE_POINTER:
            return &ffi_type_pointer;
        default:
            set_errorf(4, "Unknown Type id %x", id);
    }
}

void cast(Register *reg, int from, int to) {
    switch (from) {
        case FFI_TYPE_VOID:
            fatal("Can't cast from void");
        case FFI_TYPE_POINTER:
            fatal("Can't cast from pointer");
        case FFI_TYPE_SINT8:
            reg->value_sint64 = (int64_t) reg->value_sint8;
            goto int64;
        case FFI_TYPE_SINT16:
            reg->value_sint64 = (int64_t) reg->value_sint16;
            goto int64;
        case FFI_TYPE_SINT32:
            reg->value_sint64 = (int64_t) reg->value_sint32;
            goto int64;
        case FFI_TYPE_SINT64:
            goto int64;
        default:
            fatal("Unknown cast source %d", from);
    }

    int64:
    switch (to) {
        case FFI_TYPE_VOID:
            fatal("Can't cast to void");
        case FFI_TYPE_POINTER:
            fatal("Can't cast to pointer");
        case FFI_TYPE_SINT8:
            reg->value_sint8 = (int8_t) reg->value_sint64;
            break;
        case FFI_TYPE_SINT16:
            reg->value_sint16 = (int16_t) reg->value_sint64;
            break;
        case FFI_TYPE_SINT32:
            reg->value_sint32 = (int32_t) reg->value_sint64;
            break;
        case FFI_TYPE_SINT64:
            break;
        default:
            fatal("Unknown cast target %d", from);
    }
}


struct Bytecode {
    Register *constants;
    bool *free_map;
    size_t constant_count;

    Function *functions;
    size_t function_count;

    uint8_t *instructions;
    size_t instruction_count;

    size_t max_stack_size;
};


struct Interpreter {
    Register *stack;
    Register *stack_top;
    Bytecode *code;
    size_t code_index;
};


void setup(void) {
}

void bytecode_delete(Bytecode *ptr) {
    if (ptr == NULL) {
        return;
    }
    if (ptr->constants != NULL) {
        for (size_t i = 0; i < ptr->constant_count; ++i) {
            if (ptr->free_map[i]) {
                free(ptr->constants[i].ptr);
            }
        }
        free(ptr->constants);
        free(ptr->free_map);
    }
    if (ptr->functions != NULL) {
        free(ptr->functions);
    }
    if (ptr->instructions != NULL) {
        free(ptr->instructions);
    }
    free(ptr);
}

Function function_from_json(cJSON *ele) {
    cJSON *dll = cJSON_GetObjectItem(ele, "dll");
    if (dll == NULL || !cJSON_IsString(dll)) {
        set_error(2, "Expected dll key for function");
        return NULL;
    }
    cJSON *name = cJSON_GetObjectItem(ele, "name");
    if (name == NULL || !cJSON_IsString(name)) {
        set_error(2, "Expected name key for function");
        return NULL;
    }
    return load_function(cJSON_GetStringValue(dll), cJSON_GetStringValue(name));
}

Bytecode *bytecode_from_json(char *string) {
    cJSON *json = cJSON_Parse(string);
    if (json == NULL) {
        const char *error = cJSON_GetErrorPtr() - 10;
        if (error < string) { error = string; }
        set_errorf(2, "Could not load json. Context: %.20s", error);
        return NULL;
    }
    if (!cJSON_IsObject(json)) {
        set_error(2, "Expected Object as top level json element");
        cJSON_Delete(json);
        return NULL;
    }
    Bytecode *result = malloc(sizeof(*result));
    memset(result, 0, sizeof(*result));
    if (result == NULL) {
        set_error(1, "Failed to allocate memory for result");
        return NULL;
    }
    cJSON *item = NULL;
    if ((item = cJSON_GetObjectItem(json, "constants")) != NULL) {
        if (!cJSON_IsArray(item)) {
            set_error(2, "Expected Array as entry for \"constants\"");
            bytecode_delete(result);
            cJSON_Delete(json);
            return NULL;
        }
        result->constant_count = cJSON_GetArraySize(item);
        result->constants = calloc(result->constant_count, sizeof(*result->constants));
        result->free_map = calloc(result->constant_count, sizeof(*result->free_map));
        size_t i = 0;
        cJSON *ele;
        cJSON_ArrayForEach(ele, item) {
            if (cJSON_IsString(ele)) {
                const char *value = cJSON_GetStringValue(ele);
                char *new = malloc(strlen(value));
                if (new == NULL) {
                    set_error(1, "Failed to allocate memory for result (string constant)");
                    bytecode_delete(result); // at this point 'constants' and 'free_map' should be valid enough
                    cJSON_Delete(json);
                    return NULL;
                }
                strcpy(new, value);
                result->constants[i].ptr = new;
                result->free_map[i] = true;
            } else if (cJSON_IsNumber(ele)) {
                result->constants[i].value_double = cJSON_GetNumberValue(ele);
            }
            i++;
        }
    }
    if ((item = cJSON_GetObjectItem(json, "functions")) != NULL) {
        if (!cJSON_IsArray(item)) {
            set_error(2, "Expected Array as entry for \"functions\"");
            bytecode_delete(result);
            cJSON_Delete(json);
            return NULL;
        }
        result->function_count = cJSON_GetArraySize(item);
        result->functions = calloc(result->function_count, sizeof(*result->functions));
        size_t i = 0;
        cJSON *ele;
        cJSON_ArrayForEach(ele, item) {
            Function fun = function_from_json(ele);
            if (fun == NULL) {
                if (!has_error()) {
                    set_error(3, "function_from_json failed without message");
                }
                bytecode_delete(result);
                cJSON_Delete(json);
                return NULL;
            }
            result->functions[i] = fun;
            i++;
        }
    }
    if ((item = cJSON_GetObjectItem(json, "instructions")) != NULL) {
        if (!cJSON_IsArray(item)) {
            set_error(2, "Expected Array as entry for \"instructions\"");
            bytecode_delete(result);
            cJSON_Delete(json);
            return NULL;
        }
        result->instruction_count = cJSON_GetArraySize(item);
        result->instructions = calloc(result->instruction_count, sizeof(*result->instructions));
        size_t i = 0;
        cJSON *ele;
        cJSON_ArrayForEach(ele, item) {
            if (!cJSON_IsNumber(ele)) {
                set_error(2, "Expected number in instruction array");
                bytecode_delete(result);
                cJSON_Delete(json);
                return NULL;
            }
            int value = (int) cJSON_GetNumberValue(ele);
            if (value < 0 || value > 255) {
                set_error(2, "Expected number between 0 and 255 in instruction array");
                bytecode_delete(result);
                cJSON_Delete(json);
                return NULL;
            }
            result->instructions[i] = (uint8_t) value;
            i++;
        }
    }
    cJSON_Delete(json);
    return result;
}


Interpreter *interpreter_for_bytecode(Bytecode *bytecode) {
    Interpreter *result = malloc(sizeof(*result));
    if (result == NULL) {
        return NULL;
    }
    result->code = bytecode;
    result->stack = calloc(bytecode->max_stack_size + 1, sizeof(*result->stack));
    if (result->stack == NULL) {
        free(result);
        return NULL;
    }
    result->stack_top = result->stack;
    result->code_index = 0;
    return result;
}

void interpreter_delete(Interpreter *interpreter) {
    if (interpreter == NULL) {
        return;
    }
    free(interpreter->stack);
    free(interpreter);
}

enum Instructions {
    NOP = 0,
    LOAD_CONSTANT = 1,
    DISCARD,
    START_CALL,
    ARGUMENT,
    CALL_FUNCTION,
    CALL_VOID_FUNCTION,
    CALL_VAR,
    CALL_VOID_VAR,
};


void hello(void) {
    Function puts = load_function("msvcrt", "printf");
    ffi_cif cif;
    ffi_type *args[2];
    void *values[2];
    char *f = "printf: %s\n";
    char *s;
    ffi_arg rc;
    // Initialize the argument info vectors
    args[0] = &ffi_type_pointer;
    args[1] = &ffi_type_pointer;
    values[0] = &f;
    values[1] = &s;
    // Initialize the cif
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
                     &ffi_type_sint, args) == FFI_OK) {
        s = "Hello World!";
        ffi_call(&cif, puts, &rc, values);
        // rc now holds the result of the call to puts
        // values holds a pointer to the function’s arg, so to
        // call puts() again all we need to do is change the
        // value of s
        s = "This is cooler!";
        ffi_call(&cif, puts, &rc, values);
    }


}


struct CallInfo {
    Function func;
    ffi_cif cif;
    ffi_type **args;
    void **values;
    ffi_arg rc;
};

struct CallInfo *create_call_info(Function func, size_t argc) {
    struct CallInfo *result = malloc(sizeof(*result));
    if (result == NULL) {
        set_error(1, "Failed to allocate memory for CallInfo");
        return NULL;
    }
    result->func = func;
    result->args = calloc(argc, sizeof(*result->args)); // NOLINT(bugprone-sizeof-expression)
    result->values = calloc(argc, sizeof(*result->values));
    if (result->args == NULL || result->values == NULL) {
        set_error(1, "Failed to allocate memory for CallInfo");
        return NULL;
    }
    return result;
}

ffi_arg use_call_info_var(struct CallInfo *ci, ffi_type *ret, size_t total_count, size_t fixed_count) {
    ffi_arg res;
    if (ffi_prep_cif_var(&ci->cif, FFI_DEFAULT_ABI, fixed_count, total_count, ret, ci->args) != FFI_OK) {
        set_error(5, "ffi_prep_cif_var failed");
        res = 0;
    } else {
        ffi_call(&ci->cif, ci->func, &ci->rc, ci->values);
        res = ci->rc;
    }
    free(ci->values);
    free(ci->args);
    free(ci);
    return res;
}

void interpreter_run(Interpreter *interpreter) {
#define ARG() (interpreter->code->instructions[interpreter->code_index++])
#define PUSH() (interpreter->stack_top++)
#define POP(value) (--interpreter->stack_top)
#define PEEK(i) (interpreter->stack_top - ((i) + 1))

    Function fun;
    struct CallInfo *ci;
    size_t arg1, arg2;
    ffi_arg result;
    while (interpreter->code_index < interpreter->code->instruction_count) {
        switch (ARG()) {
            case NOP:
                break;
            case LOAD_CONSTANT:
                memcpy(PUSH(),
                       &interpreter->code->constants[ARG()],
                       sizeof(Register));
                break;
            case DISCARD:
                interpreter->stack_top -= ARG();
                break;
            case START_CALL:
                fun = interpreter->code->functions[ARG()];
                ci = create_call_info(fun, ARG());
                if (ci == NULL) { return; }
                PUSH()->ptr = ci;
                break;
            case ARGUMENT:
                arg1 = ARG();
                ci = PEEK(arg1 + 1)->ptr;
                ci->values[arg1] = PEEK(0);
                ci->args[arg1] = get_ffi_type(ARG());
                break;
            case CALL_VAR:
                arg1 = ARG();
                arg2 = ARG();
                ci = PEEK(arg1)->ptr;
                result = use_call_info_var(ci, get_ffi_type(ARG()), arg1, arg2);
                interpreter->stack_top -= arg1 + 1;
                break;
            default:
                set_error(6, "Unkown Instructionn");
                break;
        }
        if (has_error()) { return; }
    }
#undef ARG
#undef PUSH
#undef POP
#undef PEEK
}
