cmake_minimum_required(VERSION 3.20)
project(embedded_interpreter C)
set(CMAKE_C_STANDARD 99)

set(interpreter_output ${PROJECT_SOURCE_DIR}/output)

add_library(embedded_interpreter SHARED library.c)

find_library(FFI ffi x86_64-w64-mingw32/.libs)

target_include_directories(embedded_interpreter PRIVATE x86_64-w64-mingw32/include)
target_link_libraries(embedded_interpreter ${FFI})


add_executable(test_main test_main.c)

target_link_libraries(test_main embedded_interpreter)

add_custom_command(TARGET test_main POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy cygembedded_interpreter.dll ${interpreter_output}/. )
add_custom_command(TARGET test_main POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy test_main.exe ${interpreter_output}/. )
add_custom_command(TARGET test_main POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/x86_64-w64-mingw32/.libs/libffi-6.dll ${interpreter_output}/. )