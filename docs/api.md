# C/C++ Embedding API

The Akar Embedding API lets you run Akar scripts from C/C++ applications. It provides both a C-style API (with opaque handles) and a C++ RAII wrapper.

## Quick Start

### C API

```c
#include "akar/api/akar.h"

int main() {
    akar_VM* vm = akar_new_vm();
    
    // Set a global variable
    akar_push_number(vm, 42.0);
    akar_set_global(vm, "answer");
    
    // Execute script
    akar_Error err = akar_exec(vm, "print(answer)");
    if (err != AKAR_OK) {
        printf("Error: %s\n", akar_last_error(vm));
    }
    
    akar_free_vm(vm);
    return 0;
}
```

### C++ Wrapper

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

---

## VM Lifecycle

### `akar_new_vm()`

Creates a new VM instance with all built-in functions registered.

```c
akar_VM* vm = akar_new_vm();
```

### `akar_free_vm(vm)`

Frees a VM and all its resources.

```c
akar_free_vm(vm);
```

---

## Execution

### `akar_exec(vm, source)`

Execute source code. Returns `AKAR_OK` on success.

```c
akar_Error err = akar_exec(vm, "let x = 10\nprint(x)");
```

### `akar_exec_file(vm, path)`

Execute a `.ak` or `.ako` file.

```c
akar_Error err = akar_exec_file(vm, "game_logic.ak");
```

### `akar_last_error(vm)`

Get the last error message. Valid until the next API call.

```c
if (akar_exec(vm, code) != AKAR_OK) {
    printf("Error: %s\n", akar_last_error(vm));
}
```

### Error Codes

| Code | Value | Description |
|------|-------|-------------|
| `AKAR_OK` | 0 | Success |
| `AKAR_COMPILE_ERROR` | 1 | Syntax/compile error |
| `AKAR_RUNTIME_ERROR` | 2 | Runtime error |
| `AKAR_TYPE_ERROR` | 3 | Type mismatch |
| `AKAR_INDEX_ERROR` | 4 | Index out of bounds |
| `AKAR_MEMORY_ERROR` | 5 | Memory limit exceeded |

---

## Stack Operations

The API uses an auxiliary stack to pass values between C++ and Akar.

### Push Values

```c
akar_push_nil(vm);
akar_push_bool(vm, 1);          // true
akar_push_number(vm, 3.14);
akar_push_string(vm, "hello");
akar_push_stringn(vm, str, len); // with explicit length
akar_push_interned(vm, "key");   // interned string
```

### Read Values

```c
// Index -1 = top of stack, -2 = second from top, etc.
// Positive index = absolute position.

akar_Type t = akar_type(vm, -1);  // type of top value

int is_num = akar_is_number(vm, -1);
int is_str = akar_is_string(vm, -1);
int is_arr = akar_is_array(vm, -1);
int is_map = akar_is_map(vm, -1);
int is_fn  = akar_is_function(vm, -1);
int is_nil = akar_is_nil(vm, -1);
int is_bool = akar_is_bool(vm, -1);
int is_inst = akar_is_instance(vm, -1);

double num = akar_to_number(vm, -1);
int    ival = akar_to_int(vm, -1);
int    bval = akar_to_bool(vm, -1);
const char* str = akar_to_string(vm, -1);  // valid until next API call
```

### Stack Management

```c
int size = akar_stack_size(vm);
akar_pop(vm, 1);           // pop 1 value
akar_push_copy(vm, -1);    // duplicate top
```

### Type Tags

```c
enum akar_Type {
    AKAR_TYPE_NIL = 0,
    AKAR_TYPE_BOOL,
    AKAR_TYPE_NUMBER,
    AKAR_TYPE_STRING,
    AKAR_TYPE_ARRAY,
    AKAR_TYPE_MAP,
    AKAR_TYPE_FUNCTION,
    AKAR_TYPE_CLOSURE,
    AKAR_TYPE_CLASS,
    AKAR_TYPE_INSTANCE,
    AKAR_TYPE_NATIVE,
    AKAR_TYPE_FIBER,
    AKAR_TYPE_ITERATOR,
};
```

---

## Global Variables

### Set Global

Push value, then set it as a global (pops the value):

```c
akar_push_number(vm, 42.0);
akar_set_global(vm, "answer");
```

### Get Global

Gets a global and pushes it onto the stack:

```c
akar_Type t = akar_get_global(vm, "answer");
if (t == AKAR_TYPE_NUMBER) {
    double val = akar_to_number(vm, -1);
}
akar_pop(vm, 1);
```

---

## Function Calls

### Call by Name

Arguments must be pushed onto the stack before calling. Results are pushed after.

```c
// Call factorial(10)
akar_push_number(vm, 10.0);
int nresults = akar_call(vm, "factorial", 1);

if (nresults > 0) {
    double result = akar_to_number(vm, -1);
    printf("Result: %f\n", result);
    akar_pop(vm, nresults);
}
```

### Call Function Reference

```c
// Get function from global
akar_get_global(vm, "my_func");
int func_idx = akar_stack_size(vm) - 1;

// Push args
akar_push_number(vm, 42);
int nresults = akar_call_ref(vm, func_idx, 1);
akar_pop(vm, nresults + 1);  // pop result + function ref
```

---

## Register Native Functions

### Basic Registration

```c
int my_add(akar_VM* vm, int argc, int argv_base) {
    double a = akar_to_number(vm, argv_base);
    double b = akar_to_number(vm, argv_base + 1);
    akar_push_number(vm, a + b);
    return 1;  // 1 return value
}

akar_register(vm, "add", my_add);
// Now scripts can call: add(3, 4)
```

### With Userdata

```c
int my_func(akar_VM* vm, void* userdata, int argc, int argv_base) {
    MyState* state = (MyState*)userdata;
    // ... use state ...
    akar_push_number(vm, result);
    return 1;
}

MyState state;
akar_register_ud(vm, "my_func", my_func, &state);
```

### Native Function Signature

```c
// Return value: number of values pushed onto the stack
// argc: number of arguments
// argv_base: stack index of first argument
int (*akar_NativeFn)(akar_VM* vm, int argc, int argv_base);
```

Access arguments:

```c
int my_func(akar_VM* vm, int argc, int argv_base) {
    // argv_base + 0 = first arg
    // argv_base + 1 = second arg
    // etc.
    double a = akar_to_number(vm, argv_base);
    const char* s = akar_to_string(vm, argv_base + 1);
    
    // Push return value(s)
    akar_push_number(vm, result);
    return 1;  // number of return values
}
```

---

## Arrays

### Build Array

```c
akar_array_begin(vm);
akar_array_push_number(vm, 1.0);
akar_array_push_number(vm, 2.0);
akar_array_push_number(vm, 3.0);
akar_array_push_string(vm, "hello");
akar_array_push_bool(vm, 1);
akar_array_push_nil(vm);
akar_array_end(vm);  // pushes the array onto the stack
akar_set_global(vm, "my_array");
```

### Access Array

```c
akar_get_global(vm, "my_array");

int len = akar_array_len(vm, -1);

akar_Type t = akar_array_get(vm, -1, 0);  // push arr[0]
double first = akar_to_number(vm, -1);
akar_pop(vm, 1);

akar_array_get(vm, -1, -1);  // push arr[-1] (last element)
```

### Modify Array

```c
akar_get_global(vm, "my_array");
akar_push_number(vm, 99.0);
akar_array_set(vm, -2, 0);  // arr[0] = 99 (pops value)
```

---

## Maps

### Build Map

```c
akar_map_begin(vm);
akar_map_set_number(vm, "x", 100.0);
akar_map_set_number(vm, "y", 200.0);
akar_push_string(vm, "Player1");
akar_map_set_string(vm, "name");  // pops value from stack
akar_map_set_bool(vm, "active", 1);
akar_map_end(vm);  // pushes the map onto the stack
akar_set_global(vm, "player");
```

### Access Map

```c
akar_get_global(vm, "player");
akar_Type t = akar_map_get(vm, -1, "name");  // push map["name"]
const char* name = akar_to_string(vm, -1);
akar_pop(vm, 1);
```

---

## Module Registration

Register a group of functions as a map:

```c
static int math_lerp(akar_VM* vm, int argc, int argv_base) {
    double a = akar_to_number(vm, argv_base);
    double b = akar_to_number(vm, argv_base + 1);
    double t = akar_to_number(vm, argv_base + 2);
    akar_push_number(vm, a + (b - a) * t);
    return 1;
}

static int math_clamp(akar_VM* vm, int argc, int argv_base) {
    double val = akar_to_number(vm, argv_base);
    double lo = akar_to_number(vm, argv_base + 1);
    double hi = akar_to_number(vm, argv_base + 2);
    if (val < lo) val = lo;
    if (val > hi) val = hi;
    akar_push_number(vm, val);
    return 1;
}

// NULL-terminated array
static const akar_ModuleFunc math_module[] = {
    { "lerp",  math_lerp },
    { "clamp", math_clamp },
    { NULL, NULL }
};

akar_register_module(vm, "math", math_module);
// Now scripts can call: math.lerp(0, 100, 0.5)
```

**Note:** When called as `math.lerp(0, 100, 0.5)`, the Akar compiler prepends `this` (the map) as the first argument. Your function receives `argc = 4` with `argv_base + 0` being the map. Skip it by checking `argc`:

```c
int offset = (argc == expected + 1) ? 1 : 0;  // skip 'this'
double a = akar_to_number(vm, argv_base + offset);
```

---

## Memory Management

```c
// Set memory limit (bytes). 0 = unlimited.
akar_set_memory_limit(vm, 1024 * 1024 * 64);  // 64 MB

// Get current allocation
size_t usage = akar_get_memory_usage(vm);

// Force garbage collection
akar_gc(vm);
```

---

## Debug

```c
// Enable verbose VM tracing (prints each opcode)
akar_set_verbose(vm, 1);
```

---

## C++ RAII Wrapper

The `akar_api::VM` class wraps the C API with RAII:

```cpp
#include "akar/api/akar.h"

void example() {
    akar_api::VM vm;
    
    // Execution
    vm.exec("let x = 10");
    vm.exec_file("script.ak");
    
    // Push values
    vm.push(42);              // number
    vm.push(3.14);            // double
    vm.push(true);            // bool
    vm.push("hello");         // const char*
    vm.push(std::string("s")); // std::string
    vm.push_nil();
    
    // Read values
    double n = vm.to_number(-1);
    int i = vm.to_int(-1);
    bool b = vm.to_bool(-1);
    const char* s = vm.to_string(-1);
    
    // Type checks
    vm.is_number(-1);
    vm.is_string(-1);
    
    // Globals
    vm.set_global("x");
    vm.get_global("x");
    
    // Function calls
    vm.push(10);
    int nresults = vm.call("factorial", 1);
    double result = vm.to_number(-1);
    vm.pop(nresults);
    
    // Arrays
    vm.array_begin();
    vm.array_push(1.0);
    vm.array_push(2.0);
    vm.array_end();
    vm.set_global("arr");
    
    // Memory
    vm.set_memory_limit(64 * 1024 * 1024);
    vm.gc();
    
    // Debug
    vm.set_verbose(true);
    
    // Error handling
    if (vm.exec("bad code") != AKAR_OK) {
        printf("Error: %s\n", vm.last_error());
    }
}
```

---

## Build Integration

### CMake

```cmake
# Add Akar as a subdirectory or find it
add_subdirectory(path/to/akar)

# Link against the API library
target_link_libraries(my_app PRIVATE akar_api)
```

### Manual

```bash
# Compile Akar
cd akar && mkdir build && cd build
cmake .. && make -j$(nproc)

# Link
g++ my_app.cpp -I akar/include -L akar/build -lakar_api -lakar_core -o my_app
```
