# Building from Source

## Prerequisites

- C++17 compiler (GCC 8+, Clang 7+)
- CMake 3.16+
- Linux or macOS (Windows with WSL)

## Build

```bash
cd Lang
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

This produces:

| Binary | Description |
|--------|-------------|
| `akar` | Runtime + REPL |
| `akarc` | Compiler CLI (source → .ako) |
| `akar_disasm` | .ako disassembler |
| `akar_tests` | Test suite |
| `embed_demo` | Embedding API example |

## Build Options

### Debug Build

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Disables optimizations and enables debug symbols.

### Release Build (Recommended)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Enables `-O3 -march=native -flto` for maximum performance.

## Usage

### Run a Script

```bash
./akar examples/demo.ak
```

### REPL

```bash
./akar
> print("hello")
hello
> exit
```

### Evaluate Inline

```bash
./akar -e 'print(42 + 1)'
```

### Compile to .ako

```bash
./akarc examples/demo.ak -o demo.ako
./akar demo.ako
```

### Disassemble .ako

```bash
./akar_disasm demo.ako
```

### Verbose Mode

```bash
./akar -v examples/demo.ak
```

Prints every opcode executed.

### Run Tests

```bash
./akar_tests
```

### Run Script Tests

```bash
./akar tests/test_fibers_comprehensive.ak
./akar tests/test_wide_big.ak
```

## Project Structure

```
Lang/
├── CMakeLists.txt              # Build system
├── include/akar/
│   ├── api/akar.h              # C/C++ Embedding API
│   ├── common/
│   │   ├── opcodes.h           # Opcode definitions
│   │   ├── chunk.h             # Bytecode chunk
│   │   ├── value.h             # Value type (NaN-boxed)
│   │   └── token.h             # Lexer tokens
│   ├── compiler/
│   │   ├── lexer.h             # Tokenizer
│   │   ├── ast.h               # AST node definitions
│   │   ├── parser.h            # Recursive descent parser
│   │   └── codegen.h           # Bytecode generator
│   └── vm/
│       ├── vm.h                # Virtual machine
│       ├── native.h            # Native function interface
│       └── object_file.h       # .ako reader/writer
├── src/
│   ├── common/
│   │   ├── value.cpp           # Value implementation, GC
│   │   └── chunk.cpp           # Chunk implementation
│   ├── compiler/
│   │   ├── lexer.cpp           # Tokenizer
│   │   ├── parser.cpp          # Parser
│   │   ├── ast.cpp             # AST (empty, just header data)
│   │   └── codegen.cpp         # Bytecode generator
│   ├── vm/
│   │   ├── vm.cpp              # VM execution loop
│   │   ├── native.cpp          # Built-in functions
│   │   ├── object_file.cpp     # .ako serialization
│   │   └── gc.cpp              # GC (empty, in value.cpp)
│   ├── api/
│   │   └── akar_api.cpp        # Embedding API implementation
│   ├── vm_main.cpp             # akar CLI (runtime/REPL)
│   ├── compiler_main.cpp       # akarc CLI
│   └── disasm_main.cpp         # akar_disasm CLI
├── tests/
│   ├── test_main.cpp           # Test runner
│   ├── test_lexer.cpp          # Lexer tests
│   ├── test_parser.cpp         # Parser tests
│   ├── test_vm.cpp             # VM tests (90 tests)
│   ├── test_native.cpp         # Native function tests
│   ├── test_fibers_comprehensive.ak  # Fiber tests (227 tests)
│   ├── test_wide_big.ak        # Wide register tests (15 tests)
│   └── test_nested_fibers.ak   # Nested fiber tests
├── examples/
│   ├── 01_basics.ak            # Basic syntax
│   ├── 02_control_flow.ak      # if/while/for
│   ├── 03_functions.ak         # Functions and closures
│   ├── 04_arrays_maps.ak       # Collections
│   ├── 05_classes.ak           # Classes
│   ├── 06_include.ak           # File inclusion
│   ├── 07_advanced.ak          # Advanced features
│   ├── 08_upvalues.ak          # Closures and upvalues
│   ├── benchmark.ak            # Performance benchmarks
│   ├── demo.ak                 # Full demo
│   ├── math_utils.ak           # Math utilities
│   ├── sum_primes.ak           # Sum of primes benchmark
│   └── embed_demo.cpp          # C++ embedding example
└── docs/                       # This documentation
```

## Compiler Flags

The CMakeLists.txt uses these flags:

```cmake
# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Optimization (Release)
add_compile_options(-O3 -march=native -flto)
add_link_options(-flto)
```

### `-march=native`

Generates code optimized for the build machine's CPU. If you need portable binaries, remove this flag.

### `-flto`

Link-Time Optimization. Enables cross-translation-unit inlining and optimization. Significantly improves performance.

## Embedding in Your Project

### Option 1: CMake Subdirectory

```cmake
add_subdirectory(path/to/akar)
target_link_libraries(my_app PRIVATE akar_api)
```

### Option 2: Static Library

```bash
# Build the library
cd akar && mkdir build && cd build
cmake .. && make -j$(nproc)

# Link
g++ my_app.cpp -I akar/include -L akar/build \
    -lakar_api -lakar_core -lpthread -o my_app
```

### Option 3: Include Directly

Copy the `include/` and `src/` directories into your project and add the source files to your build system.
