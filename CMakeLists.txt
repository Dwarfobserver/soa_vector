
project(soa-vector)
cmake_minimum_required(VERSION 3.9)
set(CMAKE_CXX_STANDARD 17)

enable_testing()
add_executable(tests "tests/tests.cpp")
add_test(NAME tests COMMAND tests)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(tests PUBLIC "/W3")
else ()
    target_compile_options(tests PUBLIC "-Wall" "-Wextra" "-Werror")
endif()
