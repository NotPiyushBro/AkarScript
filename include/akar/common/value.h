#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cstring>
#include <cmath>

namespace akar {

// Forward declarations
struct ObjString;
struct ObjArray;
struct ObjMap;
struct ObjFunction;
struct ObjClosure;
struct ObjClass;
struct ObjInstance;
struct ObjNative;
struct ObjUpvalue;
struct ObjFiber;

enum class ObjType {
    String, Array, Map, Function, Closure, Class, Instance, Native, Upvalue, Fiber
};

struct Obj {
    ObjType type;
    bool marked = false;
    Obj* next = nullptr; // GC linked list
    size_t alloc_size = 0; // tracked allocation size for GC sweep
    virtual ~Obj() = default;
};

// NaN-boxing: pack all Value types into 8 bytes using the IEEE 754 NaN space.
// Regular doubles use the full 64-bit range. We use quiet NaN (exponent bits 62-52 all set)
// with specific bit patterns for nil, bool, and pointers.
//
// Bit layout:
//   0xFFF8000000000000  = real quiet NaN (any double that is NaN)
//   0x7FFC000000000000  = nil
//   0x7FF8000000000001  = false
//   0x7FF8000000000002  = true
//   0x7FFC000000000000 | ptr  = object pointer (user space, bit 47 set)
//   0xFFFFC00000000000 | ptr  = object pointer (kernel space, bit 47 set)
//
// To check if a Value is a number: (bits & NAN_BASE) != NAN_BASE
// To check if a Value is a pointer: (bits & 0x0008000000000000) != 0 (bit 51)
// To check if a Value is nil/bool: bits in {NIL, FALSE, TRUE}

struct Value {
    uint64_t bits;

    // NaN-boxing constants
    static constexpr uint64_t NAN_BASE = 0x7FF8000000000000ULL;
    static constexpr uint64_t NIL_VAL  = 0x7FFC000000000000ULL;
    static constexpr uint64_t FALSE_VAL = NAN_BASE | 1;
    static constexpr uint64_t TRUE_VAL  = NAN_BASE | 2;
    // Pointer tag: bit 51 set distinguishes pointers from nil/bool
    static constexpr uint64_t PTR_TAG   = 0x0008000000000000ULL;
    static constexpr uint64_t PTR_MASK  = 0x0000FFFFFFFFFFFFULL; // lower 48 bits
    static constexpr uint64_t SIGN_BIT  = 0x8000000000000000ULL; // bit 63 for kernel pointers

    // Constructors
    Value() : bits(NIL_VAL) {}
    Value(bool b) : bits(b ? TRUE_VAL : FALSE_VAL) {}
    Value(double n) {
        if (std::isnan(n)) {
            bits = NAN_BASE; // canonical NaN (not a tagged value)
        } else {
            std::memcpy(&bits, &n, 8);
        }
    }
    Value(Obj* o) {
        auto ptr = reinterpret_cast<uint64_t>(o);
        if (ptr & SIGN_BIT) {
            // Kernel-space pointer: set bit 63 to preserve it
            bits = SIGN_BIT | NAN_BASE | PTR_TAG | (ptr & PTR_MASK);
        } else {
            // User-space pointer
            bits = NAN_BASE | PTR_TAG | (ptr & PTR_MASK);
        }
    }

    // Type checks
    bool is_nil() const { return bits == NIL_VAL; }
    bool is_bool() const { return bits == FALSE_VAL || bits == TRUE_VAL; }
    bool is_number() const { return (bits & NAN_BASE) != NAN_BASE; }
    bool is_obj() const { return (bits & NAN_BASE) == NAN_BASE && bits != NAN_BASE && !is_nil() && !is_bool(); }
    bool is_string() const;
    bool is_array() const;
    bool is_map() const;
    bool is_function() const;
    bool is_closure() const;
    bool is_class() const;
    bool is_instance() const;
    bool is_native() const;
    bool is_fiber() const;

    // Accessors
    bool get_bool() const { return bits == TRUE_VAL; }

    double get_number() const {
        double d;
        std::memcpy(&d, &bits, 8);
        return d;
    }

    Obj* as_obj() const {
        uint64_t ptr = bits & PTR_MASK;
        if (bits & SIGN_BIT) {
            // Sign-extend for kernel-space pointers
            ptr |= SIGN_BIT;
        }
        Obj* result;
        std::memcpy(&result, &ptr, sizeof(Obj*));
        return result;
    }

    // Convenience accessors
    ObjString* as_string() const;
    ObjArray* as_array() const;
    ObjMap* as_map() const;
    ObjFunction* as_function() const;
    ObjClosure* as_closure() const;
    ObjClass* as_class() const;
    ObjInstance* as_instance() const;
    ObjNative* as_native() const;
    ObjFiber* as_fiber() const;

    bool is_truthy() const;
    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }

    std::string to_string() const;
};

// Heap-allocated objects
struct ObjString : Obj {
    std::string value;
    ObjString(std::string v) : value(std::move(v)) { type = ObjType::String; }
};

struct ObjArray : Obj {
    std::vector<Value> elements;
    ObjArray() { type = ObjType::Array; }
};

struct ObjMap : Obj {
    std::unordered_map<std::string, Value> entries;
    ObjMap() { type = ObjType::Map; }
};

// Upvalue for closures
struct ObjUpvalue : Obj {
    Value* location;     // points to the slot on the stack
    Value closed;        // value after captured
    ObjUpvalue* next_upvalue = nullptr;
    ObjUpvalue(Value* loc) : location(loc) { type = ObjType::Upvalue; }
};

struct UpvalueDesc {
    uint8_t index;      // register index in enclosing scope
    bool is_local;      // true = captured from immediate enclosing, false = indirect
};

struct ObjFunction : Obj {
    int arity = 0;
    bool has_varargs = false;
    int register_count = 0;
    std::vector<uint8_t> bytecode;
    std::vector<Value> constants;
    std::string name;
    int line = 0;
    std::vector<UpvalueDesc> upvalue_descs;
    ObjFunction() { type = ObjType::Function; }
};

struct ObjClosure : Obj {
    ObjFunction* function;
    std::vector<ObjUpvalue*> upvalues;
    ObjClosure(ObjFunction* func) : function(func) { type = ObjType::Closure; }
};

struct ObjClass : Obj {
    std::string name;
    std::unordered_map<std::string, Value> methods;
    ObjClass(std::string n) : name(std::move(n)) { type = ObjType::Class; }
};

struct ObjInstance : Obj {
    ObjClass* klass;
    std::unordered_map<std::string, Value> fields;
    ObjInstance(ObjClass* c) : klass(c) { type = ObjType::Instance; }
};

using NativeFn = std::function<Value(int arg_count, Value* args)>;

struct ObjNative : Obj {
    NativeFn function;
    std::string name;
    ObjNative(NativeFn fn, std::string n) : function(std::move(fn)), name(std::move(n)) {
        type = ObjType::Native;
    }
};

// Fiber (coroutine) - saved execution state
struct ObjFiber : Obj {
    enum class State { Created, Running, Suspended, Done };
    State state = State::Created;
    ObjClosure* entry = nullptr;
    Value yielded_value;
    Value resume_value;
    // Initial args from fiber_create (passed on first resume)
    std::vector<Value> initial_args;
    // Saved stack state (saved on yield, restored on resume)
    std::vector<Value> saved_stack;
    int saved_stack_top = 0;
    int saved_frame_count = 0;
    int saved_caller_stack_top = 0;
    ObjUpvalue* saved_open_upvalues = nullptr;
    struct SavedFrame {
        int base_register;
        int return_register;
        int callee_stack_pos;
        int caller_stack_top;
        int ip_offset; // offset into bytecode
    };
    std::vector<SavedFrame> saved_frames;
    ObjFiber* parent = nullptr;
    int resume_return_reg = 0;
    int frame_base = 0;  // frame index where this fiber's frames start
    ObjFiber() { type = ObjType::Fiber; }
};

// String interning / allocation helpers
ObjString* allocate_string(std::string value);
ObjArray* allocate_array();
ObjMap* allocate_map();
ObjFunction* allocate_function();
ObjClosure* allocate_closure(ObjFunction* func);
ObjClass* allocate_class(std::string name);
ObjInstance* allocate_instance(ObjClass* klass);
ObjNative* allocate_native(NativeFn fn, std::string name);
ObjUpvalue* allocate_upvalue(Value* slot);
ObjFiber* allocate_fiber();

// Global string table for interning
class StringTable {
public:
    ObjString* intern(const std::string& s);
    // Mark all interned strings as GC roots
    void mark_all();
private:
    std::unordered_map<std::string, ObjString*> strings_;
};

StringTable& get_string_table();

// Memory tracking
size_t get_allocated_bytes();
void reset_allocated_bytes();
void free_all_objects();
bool memory_limit_exceeded();
void set_memory_limit(size_t limit);
size_t get_memory_limit();
size_t get_next_gc();
void set_next_gc(size_t threshold);

// Garbage collection (mark-sweep)
void gc_mark_object(Obj* obj);
void gc_mark_value(Value val);
void gc_trace_references(Obj* obj);
void gc_sweep();
size_t gc_drain_gray_stack();
void gc_mark_string_table();

} // namespace akar
