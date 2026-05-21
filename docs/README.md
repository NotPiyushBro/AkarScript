# Akar Script Documentation

Akar Script is a fast, embeddable scripting language designed for game engines and applications. It features a register-based bytecode VM with NaN-boxing, incremental GC, coroutines (fibers), operator overloading, and a C/C++ embedding API.

## Table of Contents

| Document | Description |
|----------|-------------|
| [Language Reference](language.md) | Syntax, types, control flow, classes, closures |
| [Standard Library](stdlib.md) | Built-in functions, math, strings, arrays, maps, fibers |
| [C/C++ Embedding API](api.md) | How to embed Akar in your C/C++ application |
| [Operator Overloading](operators.md) | Magic methods for custom types |
| [VM Internals](vm.md) | Bytecode format, opcodes, GC, performance |
| [Building](building.md) | How to build from source |
| [Object File Format](object-file.md) | `.ako` binary format specification |
| [Examples](examples.md) | Annotated examples and tutorials |

## Quick Start

### Running Scripts

```bash
# Run a .ak source file
./build/akar examples/demo.ak

# Evaluate inline code
./build/akar -e 'print("hello")'

# Run compiled .ako bytecode
./build/akarc examples/demo.ak -o demo.ako
./build/akar demo.ako

# REPL
./build/akar
```

### Embedding in C++

```cpp
#include "akar/api/akar.h"

int main() {
    akar_VM* vm = akar_new_vm();
    akar_exec(vm, "print('hello from Akar!')");
    akar_free_vm(vm);
}
```

### C++ RAII Wrapper

```cpp
#include "akar/api/akar.h"

int main() {
    akar_api::VM vm;
    vm.exec("print('hello')");
    vm.push(42);
    vm.set_global("answer");
    vm.exec("print(answer)");
}
```
