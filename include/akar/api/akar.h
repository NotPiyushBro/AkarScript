// Akar Script Embedding API
// ==========================
// A clean C-style API for embedding Akar Script into C/C++ applications.
// All types use opaque handles. Thread-safe: each VM is independent.
//
// Quick start:
//   akar_VM* vm = akar_new_vm();
//   akar_push_number(vm, 42.0);
//   akar_set_global(vm, "x");
//   akar_exec(vm, "print(x + 1)");
//   akar_free_vm(vm);
//
// C++ RAII wrapper:
//   akar::VM vm;
//   vm.exec("print('hello')");

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────
// Opaque types
// ─────────────────────────────────────────────────────────────

typedef struct akar_VM akar_VM;
typedef struct akar_Array akar_Array;
typedef struct akar_Map akar_Map;
typedef struct akar_Fiber akar_Fiber;
typedef struct akar_ClassBuilder akar_ClassBuilder;

// ─────────────────────────────────────────────────────────────
// Error codes
// ─────────────────────────────────────────────────────────────

typedef enum {
    AKAR_OK = 0,
    AKAR_COMPILE_ERROR = 1,
    AKAR_RUNTIME_ERROR = 2,
    AKAR_TYPE_ERROR = 3,
    AKAR_INDEX_ERROR = 4,
    AKAR_MEMORY_ERROR = 5,
} akar_Error;

// ─────────────────────────────────────────────────────────────
// Value type tags (mirrors internal ObjType + primitives)
// ─────────────────────────────────────────────────────────────

typedef enum {
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
} akar_Type;

// ─────────────────────────────────────────────────────────────
// Native function signature
//   argc: number of arguments
//   argv: array of arguments (indices 0..argc-1 on the stack)
//   Returns: number of return values pushed (0 = nil return)
// ─────────────────────────────────────────────────────────────

typedef int (*akar_NativeFn)(akar_VM* vm, int argc, int argv_base);

// ─────────────────────────────────────────────────────────────
// Callbacks for class field access
// ─────────────────────────────────────────────────────────────

// Called when script reads obj.field. Push the value.
typedef void (*akar_FieldGetter)(akar_VM* vm, void* userdata);

// Called when script writes obj.field = value. Value at stack[-1].
typedef void (*akar_FieldSetter)(akar_VM* vm, void* userdata);

// Destructor for userdata when the instance is GC'd
typedef void (*akar_UserdataDtor)(void* userdata);

// ─────────────────────────────────────────────────────────────
// VM lifecycle
// ─────────────────────────────────────────────────────────────

// Create a new VM with all built-in functions registered.
akar_VM* akar_new_vm(void);

// Free a VM and all its resources.
void akar_free_vm(akar_VM* vm);

// ─────────────────────────────────────────────────────────────
// Execution
// ─────────────────────────────────────────────────────────────

// Execute source code. Returns AKAR_OK on success.
akar_Error akar_exec(akar_VM* vm, const char* source);

// Execute a .ako object file. Returns AKAR_OK on success.
akar_Error akar_exec_file(akar_VM* vm, const char* path);

// Get the last error message (valid until next akar_* call).
const char* akar_last_error(akar_VM* vm);

// ─────────────────────────────────────────────────────────────
// Stack operations — push values onto the VM stack
// ─────────────────────────────────────────────────────────────

void akar_push_nil(akar_VM* vm);
void akar_push_bool(akar_VM* vm, int value);
void akar_push_number(akar_VM* vm, double value);
void akar_push_string(akar_VM* vm, const char* str);
void akar_push_stringn(akar_VM* vm, const char* str, size_t len);
void akar_push_pointer(akar_VM* vm, void* ptr);   // pushes as lightuserdata (number)

// ─────────────────────────────────────────────────────────────
// Stack operations — read values from the stack
// ─────────────────────────────────────────────────────────────

// Check type at stack position (-1 = top)
akar_Type akar_type(akar_VM* vm, int index);

int         akar_is_nil(akar_VM* vm, int index);
int         akar_is_bool(akar_VM* vm, int index);
int         akar_is_number(akar_VM* vm, int index);
int         akar_is_string(akar_VM* vm, int index);
int         akar_is_array(akar_VM* vm, int index);
int         akar_is_map(akar_VM* vm, int index);
int         akar_is_function(akar_VM* vm, int index);
int         akar_is_instance(akar_VM* vm, int index);

// Read values (return 0/0.0/NULL if wrong type)
int         akar_to_bool(akar_VM* vm, int index);
double      akar_to_number(akar_VM* vm, int index);
const char* akar_to_string(akar_VM* vm, int index);  // pointer to internal buffer, valid until next VM call
int         akar_to_int(akar_VM* vm, int index);

// Pop N values from the stack
void akar_pop(akar_VM* vm, int count);

// Get current stack size
int akar_stack_size(akar_VM* vm);

// Duplicate the top value
void akar_push_copy(akar_VM* vm, int index);

// ─────────────────────────────────────────────────────────────
// Global variables
// ─────────────────────────────────────────────────────────────

// Set global[name] = stack top, pops the value
void akar_set_global(akar_VM* vm, const char* name);

// Push global[name] onto the stack. Returns AKAR_TYPE_NIL if not found.
akar_Type akar_get_global(akar_VM* vm, const char* name);

// ─────────────────────────────────────────────────────────────
// Function calls
// ─────────────────────────────────────────────────────────────

// Call a global function by name.
// Arguments must already be pushed onto the stack.
// Results are pushed onto the stack.
// Returns number of return values (0 = returned nil).
//
// Example:
//   akar_push_global(vm, "add");   // push function
//   akar_push_number(vm, 3);       // arg 1
//   akar_push_number(vm, 4);       // arg 2
//   int nresults = akar_call(vm, "add", 2);
//   double result = akar_to_number(vm, -1);
//   akar_pop(vm, nresults);
int akar_call(akar_VM* vm, const char* func_name, int nargs);

// Call a function reference already on the stack at `index`.
// Arguments must be pushed after it. Returns number of results.
int akar_call_ref(akar_VM* vm, int func_index, int nargs);

// ─────────────────────────────────────────────────────────────
// Register native functions
// ─────────────────────────────────────────────────────────────

// Register a C function as a global script function.
// The native function receives (vm, argc, argv_base).
// Use akar_peek*(vm, argv_base + i) to read arguments.
// Push return values onto the stack and return count.
void akar_register(akar_VM* vm, const char* name, akar_NativeFn fn);

// Register a native with extra userdata (passed as first arg to fn).
typedef int (*akar_NativeFnUd)(akar_VM* vm, void* ud, int argc, int argv_base);
void akar_register_ud(akar_VM* vm, const char* name, akar_NativeFnUd fn, void* userdata);

// ─────────────────────────────────────────────────────────────
// Array builder — construct arrays from C++
// ─────────────────────────────────────────────────────────────

// Start building an array. Returns a builder handle.
// Call akar_array_push_* to add elements, then akar_array_end to push it.
void akar_array_begin(akar_VM* vm);

// Push the completed array onto the stack.
void akar_array_end(akar_VM* vm);

// Push elements onto the array being built (must be between begin/end).
void akar_array_push_nil(akar_VM* vm);
void akar_array_push_bool(akar_VM* vm, int value);
void akar_array_push_number(akar_VM* vm, double value);
void akar_array_push_string(akar_VM* vm, const char* str);

// Array access on value at `index` on the stack.
// Pushes the element at `pos` onto the stack.
akar_Type akar_array_get(akar_VM* vm, int index, int pos);

// Set element at `pos` in array at `index` to value at stack top.
void akar_array_set(akar_VM* vm, int index, int pos);

// Get array length.
int akar_array_len(akar_VM* vm, int index);

// ─────────────────────────────────────────────────────────────
// Map builder — construct maps from C++
// ─────────────────────────���───────────────────────────────────

void akar_map_begin(akar_VM* vm);
void akar_map_end(akar_VM* vm);

// Set map[key] = value (value is at stack top, popped after set).
void akar_map_set_string(akar_VM* vm, const char* key);
void akar_map_set_number(akar_VM* vm, const char* key, double value);
void akar_map_set_bool(akar_VM* vm, const char* key, int value);

// Get map[key], pushes result onto stack.
akar_Type akar_map_get(akar_VM* vm, int index, const char* key);

// ─────────────────────────────────────────────────────────────
// Class registration — bind C++ structs as script classes
// ─────────────────────────────────────────────────────────────

// Begin defining a new class. Returns a builder.
// The class will be registered as a global with the given name.
akar_ClassBuilder* akar_class_begin(akar_VM* vm, const char* name, size_t userdata_size);

// Add a field with getter/setter callbacks.
// When script does obj.field, getter is called.
// When script does obj.field = val, setter is called.
void akar_class_field(akar_ClassBuilder* cls, const char* name,
                      akar_FieldGetter getter, akar_FieldSetter setter);

// Add a method. The native function receives the instance userdata.
// argv_base points to [instance, arg0, arg1, ...].
typedef int (*akar_MethodFn)(akar_VM* vm, void* self, int argc, int argv_base);
void akar_class_method(akar_ClassBuilder* cls, const char* name, akar_MethodFn method);

// Add a static method (no instance).
void akar_class_static(akar_ClassBuilder* cls, const char* name, akar_NativeFn method);

// Set the constructor. Called when script does ClassName(args).
// The userdata pointer is already allocated (of userdata_size).
// argv_base points to [arg0, arg1, ...].
typedef void (*akar_CtorFn)(akar_VM* vm, void* self, int argc, int argv_base);
void akar_class_ctor(akar_ClassBuilder* cls, akar_CtorFn ctor);

// Set the destructor. Called when the instance is GC'd.
void akar_class_dtor(akar_ClassBuilder* cls, akar_UserdataDtor dtor);

// Finalize and register the class as a global.
void akar_class_end(akar_ClassBuilder* cls);

// Get the userdata pointer from an instance on the stack.
void* akar_instance_userdata(akar_VM* vm, int index);

// ─────────────────────────────────────────────────────────────
// Module registration — register groups of functions
// ─────────────────────────────────────────────────────────────

typedef struct {
    const char* name;
    akar_NativeFn fn;
} akar_ModuleFunc;

// Register a module: creates a global map with functions as entries.
// funcs is a NULL-terminated array.
void akar_register_module(akar_VM* vm, const char* name, const akar_ModuleFunc* funcs);

// ─────────────────────────────────────────────────────────────
// Fiber / coroutine control from C++
// ─────────────────────────────────────────────────────────────

// Create a fiber from a function on the stack at `func_index`.
// The function (or closure) must already be pushed.
akar_Fiber* akar_fiber_create(akar_VM* vm, int func_index);

// Resume a fiber. Pass `has_value` and pushes value if true.
// Returns: 1 if yielded, 0 if done.
int akar_fiber_resume(akar_Fiber* fiber, int has_value);

// Get the yielded value (pushed onto stack after resume).
void akar_fiber_value(akar_Fiber* fiber);

// Get fiber status: "created", "running", "suspended", "done".
const char* akar_fiber_status(akar_Fiber* fiber);

// Free a fiber.
void akar_fiber_free(akar_Fiber* fiber);

// ─────────────────────────────────────────────────────────────
// Memory management
// ─────────────────────────────────────────────────────────────

void akar_set_memory_limit(akar_VM* vm, size_t bytes);
size_t akar_get_memory_usage(akar_VM* vm);
void akar_gc(akar_VM* vm);

// ─────────────────────────────────────────────────────────────
// Debug / introspection
// ─────────────────────────────────────────────────────────────

void akar_set_verbose(akar_VM* vm, int enabled);

// ─────────────────────────────────────────────────────────────
// String interning — push an interned string (deduplicated)
// ─────────────────────────────────────────────────────────────

void akar_push_interned(akar_VM* vm, const char* str);

#ifdef __cplusplus
} // extern "C"

#include <string>
#include <utility>

// ═════════════════════════════════════════════════════════════
// C++ RAII Wrapper (in akar_api namespace to avoid conflict)
// ═════════════════════════════════════════════════════════════

namespace akar_api {

class VM {
public:
    VM() : vm_(akar_new_vm()) {}
    ~VM() { if (vm_) akar_free_vm(vm_); }

    // Non-copyable
    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

    // Movable
    VM(VM&& other) noexcept : vm_(other.vm_) { other.vm_ = nullptr; }
    VM& operator=(VM&& other) noexcept {
        if (this != &other) {
            if (vm_) akar_free_vm(vm_);
            vm_ = other.vm_;
            other.vm_ = nullptr;
        }
        return *this;
    }

    // ── Execution ──
    akar_Error exec(const char* source) { return akar_exec(vm_, source); }
    akar_Error exec(const std::string& source) { return akar_exec(vm_, source.c_str()); }
    akar_Error exec_file(const char* path) { return akar_exec_file(vm_, path); }
    const char* last_error() { return akar_last_error(vm_); }

    // ── Push values ──
    void push_nil()              { akar_push_nil(vm_); }
    void push(bool v)            { akar_push_bool(vm_, v); }
    void push(int v)             { akar_push_number(vm_, (double)v); }
    void push(double v)          { akar_push_number(vm_, v); }
    void push(const char* s)     { akar_push_string(vm_, s); }
    void push(const std::string& s) { akar_push_stringn(vm_, s.data(), s.size()); }
    void push_pointer(void* p)   { akar_push_pointer(vm_, p); }

    // ── Type checks ──
    akar_Type type(int i)        { return akar_type(vm_, i); }
    bool is_nil(int i)           { return akar_is_nil(vm_, i) != 0; }
    bool is_bool(int i)          { return akar_is_bool(vm_, i) != 0; }
    bool is_number(int i)        { return akar_is_number(vm_, i) != 0; }
    bool is_string(int i)        { return akar_is_string(vm_, i) != 0; }
    bool is_array(int i)         { return akar_is_array(vm_, i) != 0; }
    bool is_map(int i)           { return akar_is_map(vm_, i) != 0; }
    bool is_function(int i)      { return akar_is_function(vm_, i) != 0; }

    // ── Read values ──
    bool        to_bool(int i)   { return akar_to_bool(vm_, i) != 0; }
    double      to_number(int i) { return akar_to_number(vm_, i); }
    int         to_int(int i)    { return akar_to_int(vm_, i); }
    const char* to_string(int i) { return akar_to_string(vm_, i); }

    // ── Stack ──
    void pop(int n = 1)          { akar_pop(vm_, n); }
    int  stack_size()            { return akar_stack_size(vm_); }

    // ── Globals ──
    void set_global(const char* name) { akar_set_global(vm_, name); }
    akar_Type get_global(const char* name) { return akar_get_global(vm_, name); }

    // ── Function calls ──
    int call(const char* name, int nargs) { return akar_call(vm_, name, nargs); }

    // ── Register natives ──
    void register_fn(const char* name, akar_NativeFn fn) { akar_register(vm_, name, fn); }

    // ── Array ──
    void array_begin()           { akar_array_begin(vm_); }
    void array_end()             { akar_array_end(vm_); }
    void array_push(double v)    { akar_array_push_number(vm_, v); }
    void array_push(const char* s) { akar_array_push_string(vm_, s); }
    void array_push_nil()        { akar_array_push_nil(vm_); }
    void array_push_bool(int v)  { akar_array_push_bool(vm_, v); }
    int  array_len(int i)        { return akar_array_len(vm_, i); }

    // ── Map ──
    void map_begin()             { akar_map_begin(vm_); }
    void map_end()               { akar_map_end(vm_); }
    akar_Type map_get(int i, const char* k) { return akar_map_get(vm_, i, k); }

    // ── Memory ──
    void set_memory_limit(size_t bytes) { akar_set_memory_limit(vm_, bytes); }
    size_t memory_usage()       { return akar_get_memory_usage(vm_); }
    void gc()                   { akar_gc(vm_); }

    // ── Debug ──
    void set_verbose(bool v)    { akar_set_verbose(vm_, v ? 1 : 0); }

    // ── Access raw handle ──
    akar_VM* handle()           { return vm_; }

private:
    akar_VM* vm_;
};

} // namespace akar_api

#endif // __cplusplus
