cmake_minimum_required(VERSION 3.16)
project(embedded_interpreter C)
set(CMAKE_C_STANDARD 99)

set(interpreter_output ${PROJECT_SOURCE_DIR}/output)


add_library(simple_errors STATIC
        simple_errors.c
        simple_errors.h
        )

add_library(cJSON STATIC
        cJSON/cJSON.c
        cJSON/cJSON.h
        cJSON/cJSON_Utils.c
        cJSON/cJSON_Utils.h
        )

target_compile_definitions(cJSON PUBLIC CJSON_HIDE_SYMBOLS)

add_library(embedded_interpreter SHARED embedded_interpreter.c)

target_link_libraries(embedded_interpreter PRIVATE cJSON)
target_link_libraries(embedded_interpreter PUBLIC simple_errors)
target_link_libraries(embedded_interpreter PUBLIC -llibffi)


add_executable(test_main test_main.c)

target_link_libraries(test_main embedded_interpreter)

add_custom_command(TARGET test_main POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy libembedded_interpreter.dll ${interpreter_output}/. )
add_custom_command(TARGET test_main POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy test_main.exe ${interpreter_output}/. )
