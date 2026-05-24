cmake_minimum_required(VERSION 3.16)
project(AkarScript VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings and optimization
add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native -flto)
add_link_options(-flto)

# Akar core library
add_library(akar_core STATIC
    src/common/value.cpp
    src/common/chunk.cpp
    src/compiler/lexer.cpp
    src/compiler/ast.cpp
    src/compiler/parser.cpp
    src/compiler/codegen.cpp
    src/vm/vm.cpp
    src/vm/native.cpp
    src/vm/system.cpp
    src/vm/object_file.cpp
    src/vm/gc.cpp
    src/vm/profiler.cpp
    src/vm/jit.cpp
    src/vm/trace.cpp
    src/util/repl.cpp
)

# JIT backends: compile the one matching the current architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    target_sources(akar_core PRIVATE src/vm/jit_arm64.cpp)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    # Future: target_sources(akar_core PRIVATE src/vm/jit_x86_64.cpp)
    message(STATUS "JIT: x86-64 backend not yet implemented")
else()
    message(STATUS "JIT: no backend for ${CMAKE_SYSTEM_PROCESSOR}")
endif()
target_include_directories(akar_core PUBLIC include)

# akar compiler CLI
add_executable(akarc src/compiler_main.cpp)
target_link_libraries(akarc PRIVATE akar_core)

# akar runtime/REPL
add_executable(akar src/vm_main.cpp)
target_link_libraries(akar PRIVATE akar_core)

# Disassembler
add_executable(akar_disasm src/disasm_main.cpp)
target_include_directories(akar_disasm PRIVATE include)

# Embedding API library
add_library(akar_api STATIC
    src/api/akar_api.cpp
)
target_include_directories(akar_api PUBLIC include)
target_link_libraries(akar_api PUBLIC akar_core)

# Embedding example
add_executable(embed_demo examples/embed_demo.cpp)
target_link_libraries(embed_demo PRIVATE akar_api)

# Tests
enable_testing()
add_executable(akar_tests tests/test_main.cpp)
target_link_libraries(akar_tests PRIVATE akar_core)
target_include_directories(akar_tests PRIVATE include)
add_test(NAME akar_tests COMMAND akar_tests)
