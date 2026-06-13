#pragma once
#include "akar/common/value.h"
#include "akar/common/opcodes.h"
#include "akar/vm/native.h"
#include "akar/vm/object_file.h"
#include "akar/vm/profiler.h"
#include "akar/vm/jit.h"
#include "akar/vm/trace.h"
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <string>
#include <functional>

namespace akar {

// ============================================================================
// THREAD SAFETY: The VM is NOT thread-safe. The GC, string table, object
// allocation, and all global state are shared across VM instances without
// synchronization. Use ONE VM per process/thread. Multiple VMs on different
// threads will produce data races in the allocator, GC, and string interning.
// If you need concurrent scripting, create one VM per thread and never share
// objects between VMs.
// ============================================================================

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

    // Verbose logging
    bool verbose_ = false;
    void set_verbose(bool v) { verbose_ = v; }

    // Profiling & Tracing
    Profiler profiler_;
    void set_profiling(bool on) { if (on) profiler_.start_profiling(); else profiler_.stop_profiling(); }
    void set_tracing(bool on) { if (on) profiler_.start_tracing(); else profiler_.stop_tracing(); }

    // GC
    void mark_roots();
    void collect_garbage();  // full synchronous GC (blocking)
    void gc_step();          // incremental GC step (non-blocking, processes a batch)

    // Incremental GC state
    enum class GCPhase { Idle, Marking, Sweeping };
    GCPhase gc_phase_ = GCPhase::Idle;
    static constexpr int GC_MARK_WORK = 64;   // objects to trace per incremental step
    static constexpr int GC_SWEEP_WORK = 32;  // objects to sweep per incremental step
    double gc_step_start_us_ = 0;      // GC profiling: start time of current cycle
    size_t gc_step_before_bytes_ = 0;  // GC profiling: allocated bytes before sweep

    // Fiber yield support
    bool yield_pending_ = false;
    Value yield_value_;
    ObjFiber* active_fiber_ = nullptr; // currently running fiber

    // Deferred fiber resume (set by native fiber_resume, handled by run loop)
    bool resume_pending_ = false;
    ObjFiber* resume_fiber_ = nullptr;
    Value resume_value_;
    bool resume_has_value_ = false;
    int resume_return_reg_ = 0;
    int resume_arg_count_ = 0;
    // Skip native call on fiber resume (CALL handler checks this)
    bool skip_native_call_ = false;
    Value skip_native_result_;
    // Direct fiber resume: when set, the next fiber_yield in the CALL handler
    // skips the yield and returns the resume value. More reliable than skip_native_call_
    // for interleaved fibers.
    bool fiber_resuming_ = false;
    Value fiber_resume_value_;

    // Signal/Effect tracking
    ObjEffect* current_effect_ = nullptr;  // currently executing effect (for dependency tracking)
    int effect_frame_depth_ = 0;           // frame depth when effect was entered
    std::deque<ObjEffect*> effect_queue_; // effects to re-run after current instruction
    uint16_t enum_type_counter_ = 0;      // global enum type ID counter
    uint32_t write_generation_ = 0;        // global write generation counter (for effect dedup)

    // JIT helper access (public for JIT helper functions)
    Value* jit_find_global(ObjString* name) {
        auto it = globals_.find(name);
        return it != globals_.end() ? &it->second : nullptr;
    }
    void jit_set_global_val(ObjString* name, Value val) {
        globals_[name] = val;
    }

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
    // Shared CALL handler for normal and WIDE instructions
    // Returns: 0 = continue, 1 = dispatch (yield/skip), -1 = error
    int exec_call(int a, int b);

    // Error handling
    void runtime_error(const char* format, ...);
    bool throw_to_catch(const std::string& msg);
    bool exception_caught_ = false;  // set when try/catch catches an error

    // Constants from current frame
    Value read_constant(int index);

    // Cached hash symbols for common field names (avoid recomputing on every access)
    static inline const std::string HASH_LENGTH = akar_hash_symbol("length");
    static inline const std::string HASH_INIT = akar_hash_symbol("init");

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

    // Use ObjString* as key - strings are interned so pointer equality works
    // This eliminates string hashing on every GET_GLOBAL/SET_GLOBAL
    std::unordered_map<ObjString*, Value, std::hash<ObjString*>, std::equal_to<ObjString*>> globals_;
    ObjUpvalue* open_upvalues_ = nullptr;

    std::string last_error_;

public:
    // JIT compiler
    JITCache jit_cache_;
    bool jit_enabled_ = true;

    // Trace JIT: hot loop detection and compiled traces
    std::unordered_map<uint8_t*, int> trace_hot_counts_;
    std::unordered_map<uint8_t*, JITCode*> compiled_traces_;
    TraceCompiler trace_compiler_;
    static constexpr int TRACE_COMPILE_THRESHOLD = 2000;
    bool trace_jit_enabled_ = true;

    // Direct JIT call from JIT helper (avoids vector allocation)
    Value jit_call_direct(ObjClosure* closure, Value* args, int arg_count);

    // External fiber tracking for GC (fibers created via API, not yet on VM stack)
    std::vector<ObjFiber*> external_fibers_;

    // VM registration for GC (tracks all live VMs so GC marks from all roots)
    static std::unordered_set<VM*> active_vms_;
};

} // namespace akar
