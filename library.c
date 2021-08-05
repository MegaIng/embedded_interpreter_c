#include "library.h"

#include <windows.h>
#include <stdio.h>
#include <ffi.h>

typedef void (*Function)(void);

void print_error(void) {
    LPTSTR errorText = NULL;

    FormatMessage(
            // use system message tables to retrieve error text
            FORMAT_MESSAGE_FROM_SYSTEM
            // allocate buffer on local heap for error text
            | FORMAT_MESSAGE_ALLOCATE_BUFFER
            // Important! will fail otherwise, since we're not
            // (and CANNOT) pass insertion parameters
            | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,    // unused with FORMAT_MESSAGE_FROM_SYSTEM
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &errorText,  // output
            0, // minimum size for output buffer
            NULL);   // arguments - see note

    if (NULL != errorText) {
        fputs(errorText, stderr);

        // release memory allocated by FormatMessage()
        LocalFree(errorText);
        errorText = NULL;
    } else {
        printf("Format message failed with 0x%x\n", GetLastError());
    }
}

Function load_function(const char *dll_name, const char *function_name) {
    HMODULE dll = LoadLibraryA(dll_name);
    if (dll == NULL) {
        print_error();
        exit(EXIT_FAILURE);
    }
    GetProcAddress(dll, function_name);
}

void hello(void) {
    Function puts = load_function("crt", "puts");
    ffi_cif cif;
    ffi_type *args[1];
    void *values[1];
    char *s;
    ffi_arg rc;
    /* Initialize the argument info vectors */
    args[0] = &ffi_type_pointer;
    values[0] = &s;
    /* Initialize the cif */
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
                     &ffi_type_sint, args) == FFI_OK) {
        s = "Hello World!";
        ffi_call(&cif, puts, &rc, values);
        /* rc now holds the result of the call to puts */
        /* values holds a pointer to the function’s arg, so to
        call puts() again all we need to do is change the
        value of s */
        s = "This is cooler!";
        ffi_call(&cif, puts, &rc, values);
    }


}
