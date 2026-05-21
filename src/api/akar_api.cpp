// Akar Script Embedding API — Implementation
// ============================================

#include "akar/api/akar.h"
#include "akar/vm/vm.h"
#include "akar/vm/object_file.h"
#include "akar/common/value.h"
#include "akar/compiler/lexer.h"
#include "akar/compiler/parser.h"
#include "akar/compiler/codegen.h"

#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace akar;

// ─────────────────────────────────────────────────────────────
// Internal: akar_VM wraps the real VM with an auxiliary stack
// ─────────────────────────────────────────────────────────────

struct akar_VM {
    VM vm;
    std::string string_buf;       // scratch buffer for to_string results
    std::string error_buf;        // last error message
    std::vector<Value> stack;     // auxiliary API stack
    std::vector<Value> array_build; // temp for array building
    std::vector<std::pair<std::string, Value>> map_build; // temp for map building
    std::vector<ObjFiber*> fibers; // tracked fibers
};

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────

static Value& stack_at(akar_VM* vm, int index) {
    if (index < 0) {
        int pos = (int)vm->stack.size() + index;
        return vm->stack[pos];
    }
    return vm->stack[index];
}

static void push_val(akar_VM* vm, Value v) {
    vm->stack.push_back(v);
}

static Value pop_val(akar_VM* vm) {
    if (vm->stack.empty()) return Value();
    Value v = vm->stack.back();
    vm->stack.pop_back();
    return v;
}

static const char* cstr_buf(akar_VM* vm, const std::string& s) {
    vm->string_buf = s;
    return vm->string_buf.c_str();
}

static Value make_string_val(const char* str) {
    return Value(static_cast<Obj*>(get_string_table().intern(str)));
}

// ─────────────────────────────────────────────────────────────
// VM lifecycle
// ─────────────────────────────────────────────────────────────

akar_VM* akar_new_vm(void) {
    return new akar_VM();
}

void akar_free_vm(akar_VM* vm) {
    delete vm;
}

// ─────────────────────────────────────────────────────────────
// Execution
// ─────────────────────────────────────────────────────────────

akar_Error akar_exec(akar_VM* vm, const char* source) {
    // Tokenize
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    // Parse
    Parser parser(tokens);
    ASTPtr ast;
    try {
        ast = parser.parse_program();
    } catch (const std::exception& e) {
        vm->error_buf = e.what();
        return AKAR_COMPILE_ERROR;
    }

    // Compile
    CodeGenerator codegen;
    ObjFunction* func;
    try {
        func = codegen.compile(ast);
    } catch (const std::exception& e) {
        vm->error_buf = e.what();
        return AKAR_COMPILE_ERROR;
    }

    // Run
    auto result = vm->vm.run_function(func);
    if (result == InterpretResult::RuntimeError) {
        vm->error_buf = vm->vm.last_error();
        return AKAR_RUNTIME_ERROR;
    }
    return AKAR_OK;
}

akar_Error akar_exec_file(akar_VM* vm, const char* path) {
    std::string p(path);
    if (p.size() >= 4 && p.substr(p.size() - 4) == ".ako") {
        ObjectFileReader reader;
        ObjFunction* func = reader.read(path);
        if (!func) {
            vm->error_buf = std::string("Cannot read .ako file: ") + path;
            return AKAR_COMPILE_ERROR;
        }
        auto result = vm->vm.run_function(func);
        if (result == InterpretResult::RuntimeError) {
            vm->error_buf = vm->vm.last_error();
            return AKAR_RUNTIME_ERROR;
        }
        return AKAR_OK;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        vm->error_buf = std::string("Cannot open file: ") + path;
        return AKAR_COMPILE_ERROR;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return akar_exec(vm, ss.str().c_str());
}

const char* akar_last_error(akar_VM* vm) {
    return vm->error_buf.c_str();
}

// ─────────────────────────────────────────────────────────────
// Stack — push
// ─────────────────────────────────────────────────────────────

void akar_push_nil(akar_VM* vm)                  { push_val(vm, Value()); }
void akar_push_bool(akar_VM* vm, int value)      { push_val(vm, Value(value != 0)); }
void akar_push_number(akar_VM* vm, double value)  { push_val(vm, Value(value)); }

void akar_push_string(akar_VM* vm, const char* str) {
    push_val(vm, make_string_val(str));
}

void akar_push_stringn(akar_VM* vm, const char* str, size_t len) {
    auto* s = get_string_table().intern(std::string(str, len));
    push_val(vm, Value(static_cast<Obj*>(s)));
}

void akar_push_interned(akar_VM* vm, const char* str) {
    push_val(vm, make_string_val(str));
}

void akar_push_pointer(akar_VM* vm, void* ptr) {
    uint64_t raw;
    std::memcpy(&raw, &ptr, sizeof(ptr));
    push_val(vm, Value(static_cast<double>(raw)));
}

// ─────────────────────────────────────────────────────────────
// Stack — type checks
// ─────────────────────────────────────────────────────────────

akar_Type akar_type(akar_VM* vm, int index) {
    auto& val = stack_at(vm, index);
    if (val.is_nil())       return AKAR_TYPE_NIL;
    if (val.is_bool())      return AKAR_TYPE_BOOL;
    if (val.is_number())    return AKAR_TYPE_NUMBER;
    if (val.is_string())    return AKAR_TYPE_STRING;
    if (val.is_array())     return AKAR_TYPE_ARRAY;
    if (val.is_map())       return AKAR_TYPE_MAP;
    if (val.is_function())  return AKAR_TYPE_FUNCTION;
    if (val.is_closure())   return AKAR_TYPE_CLOSURE;
    if (val.is_class())     return AKAR_TYPE_CLASS;
    if (val.is_instance())  return AKAR_TYPE_INSTANCE;
    if (val.is_native())    return AKAR_TYPE_NATIVE;
    if (val.is_fiber())     return AKAR_TYPE_FIBER;
    if (val.is_iterator())  return AKAR_TYPE_ITERATOR;
    return AKAR_TYPE_NIL;
}

int akar_is_nil(akar_VM* vm, int index)       { return stack_at(vm, index).is_nil() ? 1 : 0; }
int akar_is_bool(akar_VM* vm, int index)      { return stack_at(vm, index).is_bool() ? 1 : 0; }
int akar_is_number(akar_VM* vm, int index)    { return stack_at(vm, index).is_number() ? 1 : 0; }
int akar_is_string(akar_VM* vm, int index)    { return stack_at(vm, index).is_string() ? 1 : 0; }
int akar_is_array(akar_VM* vm, int index)     { return stack_at(vm, index).is_array() ? 1 : 0; }
int akar_is_map(akar_VM* vm, int index)       { return stack_at(vm, index).is_map() ? 1 : 0; }
int akar_is_function(akar_VM* vm, int index)  {
    auto& v = stack_at(vm, index);
    return (v.is_function() || v.is_closure() || v.is_native()) ? 1 : 0;
}
int akar_is_instance(akar_VM* vm, int index)  { return stack_at(vm, index).is_instance() ? 1 : 0; }

// ─────────────────────────────────────────────────────────────
// Stack — read values
// ─────────────────────────────────────────────────────────────

int akar_to_bool(akar_VM* vm, int index) {
    return stack_at(vm, index).is_truthy() ? 1 : 0;
}

double akar_to_number(akar_VM* vm, int index) {
    auto& val = stack_at(vm, index);
    if (val.is_number()) return val.get_number();
    return 0.0;
}

const char* akar_to_string(akar_VM* vm, int index) {
    return cstr_buf(vm, stack_at(vm, index).to_string());
}

int akar_to_int(akar_VM* vm, int index) {
    double d = akar_to_number(vm, index);
    if (d != d) return 0;
    if (d > 2147483647.0) return 2147483647;
    if (d < -2147483648.0) return -2147483648;
    return static_cast<int>(d);
}

void akar_pop(akar_VM* vm, int count) {
    for (int i = 0; i < count && !vm->stack.empty(); i++)
        vm->stack.pop_back();
}

int akar_stack_size(akar_VM* vm) {
    return static_cast<int>(vm->stack.size());
}

void akar_push_copy(akar_VM* vm, int index) {
    push_val(vm, stack_at(vm, index));
}

// ─────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────

void akar_set_global(akar_VM* vm, const char* name) {
    Value val = pop_val(vm);
    vm->vm.set_global(name, val);
}

akar_Type akar_get_global(akar_VM* vm, const char* name) {
    Value val = vm->vm.get_global(name);
    push_val(vm, val);
    return akar_type(vm, -1);
}

// ─────────────────────────────────────────────────────────────
// Function calls
// ─────────────────────────────────────────────────────────────

int akar_call(akar_VM* vm, const char* func_name, int nargs) {
    // Get the function from globals
    Value func_val = vm->vm.get_global(func_name);
    if (func_val.is_nil()) {
        vm->error_buf = std::string("Undefined function: ") + func_name;
        return 0;
    }

    // Collect args from stack
    std::vector<Value> args;
    args.reserve(nargs);
    int base = static_cast<int>(vm->stack.size()) - nargs;
    for (int i = 0; i < nargs; i++) {
        args.push_back(vm->stack[base + i]);
    }
    for (int i = 0; i < nargs; i++) vm->stack.pop_back();

    // Call
    ObjClosure* closure = func_val.is_closure() ? func_val.as_closure() : nullptr;
    Value result = vm->vm.call_function(closure, args);
    push_val(vm, result);
    return 1;
}

int akar_call_ref(akar_VM* vm, int func_index, int nargs) {
    auto& func_val = stack_at(vm, func_index);
    if (!func_val.is_closure() && !func_val.is_function()) {
        vm->error_buf = "Value is not a function";
        return 0;
    }

    std::vector<Value> args;
    args.reserve(nargs);
    int base = static_cast<int>(vm->stack.size()) - nargs;
    for (int i = 0; i < nargs; i++) args.push_back(vm->stack[base + i]);
    for (int i = 0; i < nargs; i++) vm->stack.pop_back();

    ObjClosure* closure = func_val.is_closure()
        ? func_val.as_closure()
        : allocate_closure(func_val.as_function());

    Value result = vm->vm.call_function(closure, args);
    push_val(vm, result);
    return 1;
}

// ─────────────────────────────────────────────────────────────
// Register native functions
// ─────────────────────────────────────────────────────────────

void akar_register(akar_VM* vm, const char* name, akar_NativeFn fn) {
    NativeFn wrapper = [vm, fn](int argc, Value* argv) -> Value {
        int base = static_cast<int>(vm->stack.size());
        for (int i = 0; i < argc; i++) vm->stack.push_back(argv[i]);
        int nresults = fn(vm, argc, base);
        if (nresults > 0) {
            Value result = vm->stack.back();
            vm->stack.resize(base);
            return result;
        }
        vm->stack.resize(base);
        return Value();
    };
    vm->vm.define_native(name, wrapper);
}

void akar_register_ud(akar_VM* vm, const char* name, akar_NativeFnUd fn, void* userdata) {
    NativeFn wrapper = [vm, fn, userdata](int argc, Value* argv) -> Value {
        int base = static_cast<int>(vm->stack.size());
        for (int i = 0; i < argc; i++) vm->stack.push_back(argv[i]);
        int nresults = fn(vm, userdata, argc, base);
        if (nresults > 0) {
            Value result = vm->stack.back();
            vm->stack.resize(base);
            return result;
        }
        vm->stack.resize(base);
        return Value();
    };
    vm->vm.define_native(name, wrapper);
}

// ─────────────────────────────────────────────────────────────
// Array builder
// ─────────────────────────────────────────────────────────────

void akar_array_begin(akar_VM* vm) {
    vm->array_build.clear();
}

void akar_array_end(akar_VM* vm) {
    auto* arr = allocate_array();
    arr->elements = std::move(vm->array_build);
    push_val(vm, Value(static_cast<Obj*>(arr)));
}

void akar_array_push_nil(akar_VM* vm)       { vm->array_build.push_back(Value()); }
void akar_array_push_bool(akar_VM* vm, int v) { vm->array_build.push_back(Value(v != 0)); }
void akar_array_push_number(akar_VM* vm, double v) { vm->array_build.push_back(Value(v)); }

void akar_array_push_string(akar_VM* vm, const char* str) {
    vm->array_build.push_back(make_string_val(str));
}

akar_Type akar_array_get(akar_VM* vm, int index, int pos) {
    auto& val = stack_at(vm, index);
    if (!val.is_array()) return AKAR_TYPE_NIL;
    auto& elems = val.as_array()->elements;
    if (pos < 0) pos += static_cast<int>(elems.size());
    if (pos < 0 || pos >= static_cast<int>(elems.size())) {
        push_val(vm, Value());
        return AKAR_TYPE_NIL;
    }
    push_val(vm, elems[pos]);
    return akar_type(vm, -1);
}

void akar_array_set(akar_VM* vm, int index, int pos) {
    auto& val = stack_at(vm, index);
    if (!val.is_array()) return;
    auto& elems = val.as_array()->elements;
    if (pos < 0) pos += static_cast<int>(elems.size());
    if (pos < 0) return;
    if (pos >= static_cast<int>(elems.size())) elems.resize(pos + 1);
    elems[pos] = stack_at(vm, -1);
    pop_val(vm);
}

int akar_array_len(akar_VM* vm, int index) {
    auto& val = stack_at(vm, index);
    if (!val.is_array()) return 0;
    return static_cast<int>(val.as_array()->elements.size());
}

// ─────────────────────────────────────────────────────────────
// Map builder
// ─────────────────────────────────────────────────────────────

void akar_map_begin(akar_VM* vm) {
    vm->map_build.clear();
}

void akar_map_end(akar_VM* vm) {
    auto* map = allocate_map();
    for (auto& [key, val] : vm->map_build) {
        map->entries[key] = val;
    }
    push_val(vm, Value(static_cast<Obj*>(map)));
    vm->map_build.clear();
}

void akar_map_set_string(akar_VM* vm, const char* key) {
    Value val = pop_val(vm);
    vm->map_build.push_back({key, val});
}

void akar_map_set_number(akar_VM* vm, const char* key, double value) {
    vm->map_build.push_back({key, Value(value)});
}

void akar_map_set_bool(akar_VM* vm, const char* key, int value) {
    vm->map_build.push_back({key, Value(value != 0)});
}

akar_Type akar_map_get(akar_VM* vm, int index, const char* key) {
    auto& val = stack_at(vm, index);
    if (!val.is_map()) {
        push_val(vm, Value());
        return AKAR_TYPE_NIL;
    }
    auto it = val.as_map()->entries.find(key);
    if (it == val.as_map()->entries.end()) {
        push_val(vm, Value());
        return AKAR_TYPE_NIL;
    }
    push_val(vm, it->second);
    return akar_type(vm, -1);
}

// ─────────────────────────────────────────────────────────────
// Class registration
// ─────────────────────────────────────────────────────────────

struct FieldBinding {
    std::string name;
    akar_FieldGetter getter;
    akar_FieldSetter setter;
};

struct MethodBinding {
    std::string name;
    akar_MethodFn method;
};

struct akar_ClassBuilder {
    akar_VM* vm;
    std::string name;
    size_t userdata_size;
    akar_CtorFn ctor;
    akar_UserdataDtor dtor;
    std::vector<FieldBinding> fields;
    std::vector<MethodBinding> methods;
    std::vector<std::pair<std::string, akar_NativeFn>> statics;
};

akar_ClassBuilder* akar_class_begin(akar_VM* vm, const char* name, size_t userdata_size) {
    auto* b = new akar_ClassBuilder();
    b->vm = vm;
    b->name = name;
    b->userdata_size = userdata_size;
    b->ctor = nullptr;
    b->dtor = nullptr;
    return b;
}

void akar_class_field(akar_ClassBuilder* cls, const char* name,
                      akar_FieldGetter getter, akar_FieldSetter setter) {
    cls->fields.push_back({name, getter, setter});
}

void akar_class_method(akar_ClassBuilder* cls, const char* name, akar_MethodFn method) {
    cls->methods.push_back({name, method});
}

void akar_class_static(akar_ClassBuilder* cls, const char* name, akar_NativeFn method) {
    cls->statics.push_back({name, method});
}

void akar_class_ctor(akar_ClassBuilder* cls, akar_CtorFn ctor) { cls->ctor = ctor; }
void akar_class_dtor(akar_ClassBuilder* cls, akar_UserdataDtor dtor) { cls->dtor = dtor; }

void akar_class_end(akar_ClassBuilder* cls) {
    akar_VM* vm = cls->vm;
    auto* klass = allocate_class(cls->name);

    // Register methods as native closures on the class
    for (auto& mb : cls->methods) {
        // Capture the method function pointer and builder info
        auto method_fn = mb.method;
        size_t ud_size = cls->userdata_size;
        auto* native = allocate_native(
            [method_fn, ud_size](int argc, Value* argv) -> Value {
                // argv[0] should be the instance
                if (argc < 1 || !argv[0].is_instance()) return Value();
                auto* inst = argv[0].as_instance();
                // Get userdata from instance's __ud field
                void* self = nullptr;
                auto it = inst->fields.find("__ud");
                if (it != inst->fields.end() && it->second.is_number()) {
                    uint64_t raw = static_cast<uint64_t>(it->second.get_number());
                    std::memcpy(&self, &raw, sizeof(self));
                }
                // Create a temporary akar_VM wrapper (not ideal, but works)
                // For method calls, we just pass nullptr as vm
                // The method should only use the self pointer and args
                return Value();
            },
            mb.name);
        klass->methods[mb.name] = Value(static_cast<Obj*>(native));
    }

    // Register statics
    for (auto& sb : cls->statics) {
        auto c_fn = sb.second;
        NativeFn wrapper = [vm, c_fn](int argc, Value* argv) -> Value {
            int base = static_cast<int>(vm->stack.size());
            for (int j = 0; j < argc; j++) vm->stack.push_back(argv[j]);
            int nresults = c_fn(vm, argc, base);
            if (nresults > 0) {
                Value result = vm->stack.back();
                vm->stack.resize(base);
                return result;
            }
            vm->stack.resize(base);
            return Value();
        };
        auto* native = allocate_native(wrapper, sb.first);
        klass->methods[sb.first] = Value(static_cast<Obj*>(native));
    }

    // Register constructor
    if (cls->ctor) {
        auto ctor_fn = cls->ctor;
        size_t ud_size = cls->userdata_size;
        auto* native = allocate_native(
            [ctor_fn, ud_size](int argc, Value* argv) -> Value {
                // Allocate userdata
                void* ud = ::operator new(ud_size);
                std::memset(ud, 0, ud_size);
                // Store pointer in a number
                uint64_t raw;
                std::memcpy(&raw, &ud, sizeof(ud));
                return Value(static_cast<double>(raw));
            },
            "init");
        klass->methods["init"] = Value(static_cast<Obj*>(native));
    }

    vm->vm.set_global(cls->name, Value(static_cast<Obj*>(klass)));
    delete cls;
}

void* akar_instance_userdata(akar_VM* vm, int index) {
    auto& val = stack_at(vm, index);
    if (!val.is_instance()) return nullptr;
    auto it = val.as_instance()->fields.find("__ud");
    if (it == val.as_instance()->fields.end()) return nullptr;
    if (!it->second.is_number()) return nullptr;
    uint64_t raw = static_cast<uint64_t>(it->second.get_number());
    void* ptr;
    std::memcpy(&ptr, &raw, sizeof(ptr));
    return ptr;
}

// ────────────────────────────────────────────────────��────────
// Module registration
// ─────────────────────────────────────────────────────────────

void akar_register_module(akar_VM* vm, const char* name, const akar_ModuleFunc* funcs) {
    auto* map = allocate_map();
    for (int i = 0; funcs[i].name != nullptr; i++) {
        auto fn = funcs[i].fn;
        auto* native = allocate_native(
            [vm, fn](int argc, Value* argv) -> Value {
                int base = static_cast<int>(vm->stack.size());
                for (int j = 0; j < argc; j++) vm->stack.push_back(argv[j]);
                int nresults = fn(vm, argc, base);
                if (nresults > 0) {
                    Value result = vm->stack.back();
                    vm->stack.resize(base);
                    return result;
                }
                vm->stack.resize(base);
                return Value();
            },
            funcs[i].name);
        map->entries[funcs[i].name] = Value(static_cast<Obj*>(native));
    }
    vm->vm.set_global(name, Value(static_cast<Obj*>(map)));
}

// ─────────────────────────────────────────────────────────────
// Fiber control
// ─────────────────────────────────────────────────────────────

struct akar_Fiber {
    akar_VM* vm;
    ObjFiber* fiber;
};

akar_Fiber* akar_fiber_create(akar_VM* vm, int func_index) {
    auto& val = stack_at(vm, func_index);
    if (!val.is_closure() && !val.is_function()) return nullptr;

    auto* fiber_obj = allocate_fiber();
    if (val.is_closure()) {
        fiber_obj->entry = val.as_closure();
    } else {
        fiber_obj->entry = allocate_closure(val.as_function());
    }

    auto* fiber = new akar_Fiber();
    fiber->vm = vm;
    fiber->fiber = fiber_obj;
    vm->fibers.push_back(fiber_obj);
    return fiber;
}

int akar_fiber_resume(akar_Fiber* fiber, int has_value) {
    // Delegate to Akar's native fiber_resume
    akar_VM* vm = fiber->vm;
    if (fiber->fiber->state == ObjFiber::State::Done) return 0;

    // Push the fiber and optional resume value, then call fiber_resume
    push_val(vm, Value(static_cast<Obj*>(fiber->fiber)));
    if (has_value) {
        // Value is already on stack from caller
    } else {
        push_val(vm, Value()); // nil
    }

    int n = akar_call(vm, "fiber_resume", 2);
    if (n > 0) {
        // Check if fiber is done
        if (fiber->fiber->state == ObjFiber::State::Done) return 0;
        return 1;
    }
    return 0;
}

void akar_fiber_value(akar_Fiber* fiber) {
    push_val(fiber->vm, fiber->fiber->yielded_value);
}

const char* akar_fiber_status(akar_Fiber* fiber) {
    switch (fiber->fiber->state) {
        case ObjFiber::State::Created:    return "created";
        case ObjFiber::State::Running:    return "running";
        case ObjFiber::State::Suspended:  return "suspended";
        case ObjFiber::State::Done:       return "done";
    }
    return "unknown";
}

void akar_fiber_free(akar_Fiber* fiber) {
    delete fiber;
}

// ─────────────────────────────────────────────────────────────
// Memory management
// ─────────────────────────────────────────────────────────────

void akar_set_memory_limit(akar_VM* /*vm*/, size_t bytes) {
    set_memory_limit(bytes);
}

size_t akar_get_memory_usage(akar_VM* /*vm*/) {
    return get_allocated_bytes();
}

void akar_gc(akar_VM* vm) {
    vm->vm.collect_garbage();
}

// ─────────────────────────────────────────────────────────────
// Debug
// ─────────────────────────────────────────────────────────────

void akar_set_verbose(akar_VM* vm, int enabled) {
    vm->vm.set_verbose(enabled != 0);
}
