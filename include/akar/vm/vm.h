#pragma once
#include "akar/common/value.h"
#include "akar/common/opcodes.h"
#include "akar/vm/native.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <functional>

namespace akar {

// Call frame for function calls
struct CallFrame {
    ObjClosure* closure = nullptr;
    uint8_t* ip = nullptr;       // instruction pointer
    Value* slots = nullptr;      // base of registers for this frame
    int base_register = 0;
    int return_register = 0;
    int callee_stack_pos = 0;   // absolute stack position of callee (for restoring on return)
    int caller_stack_top = 0;   // caller's stack_top before call (for restoring on return)
};

// Range iterator for "for i in start..end"
struct RangeIterator {
    double current;
    double end;
    double step;
    bool done;
};

enum class InterpretResult {
    Ok,
    CompileError,
    RuntimeError
};

class VM {
public:
    VM();
    ~VM();

    InterpretResult interpret(const std::string& source);
    InterpretResult run_function(ObjFunction* function);
    InterpretResult run_bytecode(const std::vector<uint8_t>& bytecode, const std::vector<Value>& constants);

    // Call a script function from C++
    Value call_function(ObjClosure* closure, const std::vector<Value>& args);

    // Register native functions
    void define_native(const std::string& name, NativeFn function);

    // Global variable access
    void set_global(const std::string& name, Value value);
    Value get_global(const std::string& name) const;

    // Get last error
    const std::string& last_error() const { return last_error_; }

    // GC
    void mark_roots();
    void collect_garbage();

    // Fiber yield support (used by native yield function)
    bool yield_pending_ = false;
    Value yield_value_;

private:
    InterpretResult run();

    // Stack/register operations
    Value& slot(int index);
    void push(Value value);
    Value pop();

    // Frame management
    bool call(ObjClosure* closure, int arg_count, int return_reg, int callee_abs);
    bool call_native(ObjNative* native, int arg_count, int return_reg, int callee_abs);
    void pop_frame();

    // Error handling
    void runtime_error(const char* format, ...);
    bool throw_to_catch(const std::string& msg);
    bool exception_caught_ = false;  // set when try/catch catches an error

    // Constants from current frame
    Value read_constant(int index);

    // Stack
    static constexpr int MAX_STACK = 256 * 256;
    static constexpr int MAX_FRAMES = 256;
    static constexpr int MAX_TRY = 64;

    Value stack_[MAX_STACK];

    // Try-catch frames
    struct TryFrame {
        int frame_count;       // call frame count at try entry
        uint8_t* catch_ip;     // where to jump on throw
        int catch_register;    // where to store exception value
    };
    TryFrame try_frames_[MAX_TRY];
    int try_count_ = 0;
    int stack_top_ = 0;

    CallFrame frames_[MAX_FRAMES];
    int frame_count_ = 0;

    std::unordered_map<std::string, Value> globals_;
    ObjUpvalue* open_upvalues_ = nullptr;

    std::string last_error_;

    // VM registration for GC (tracks all live VMs so GC marks from all roots)
    static std::unordered_set<VM*> active_vms_;
};

} // namespace akar
