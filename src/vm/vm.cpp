#include "akar/vm/vm.h"
#include "akar/vm/object_file.h"
#include "akar/compiler/lexer.h"
#include "akar/compiler/parser.h"
#include "akar/compiler/codegen.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace akar {

std::unordered_set<VM*> VM::active_vms_;

VM::VM() {
    stack_top_ = 0;
    frame_count_ = 0;
    open_upvalues_ = nullptr;
    active_vms_.insert(this);
    register_builtins(*this);
}

VM::~VM() {
    active_vms_.erase(this);
    // If this is the last VM, free all objects
    if (active_vms_.empty()) {
        free_all_objects();
    }
}

void VM::mark_roots() {
    // Find the maximum stack extent across all frames
    int max_stack = stack_top_;
    for (int i = 0; i < frame_count_; i++) {
        // Each frame's registers start at base_register and extend for register_count
        // The caller's registers are below base_register and still live on the stack
        int frame_end = frames_[i].base_register + frames_[i].closure->function->register_count;
        if (frame_end > max_stack) max_stack = frame_end;
    }
    // Clamp to stack size
    if (max_stack > MAX_STACK) max_stack = MAX_STACK;
    // Mark the entire live stack region
    for (int i = 0; i < max_stack; i++) {
        gc_mark_value(stack_[i]);
    }
    // Mark globals
    for (auto& [name, val] : globals_) {
        gc_mark_object(static_cast<Obj*>(name)); // mark the key (ObjString*) too
        gc_mark_value(val);
    }
    // Mark closures in call frames
    for (int i = 0; i < frame_count_; i++) {
        gc_mark_object(static_cast<Obj*>(frames_[i].closure));
    }
    // Mark open upvalues
    for (ObjUpvalue* uv = open_upvalues_; uv != nullptr; uv = uv->next_upvalue) {
        gc_mark_object(static_cast<Obj*>(uv));
    }
    // Mark yield value
    gc_mark_value(yield_value_);
    // Mark resume value and skip_native_result (can hold object references during fiber ops)
    gc_mark_value(resume_value_);
    gc_mark_value(skip_native_result_);
    // Mark active fiber and pending resume fiber
    if (active_fiber_) {
        gc_mark_object(static_cast<Obj*>(active_fiber_));
    }
    if (resume_fiber_) {
        gc_mark_object(static_cast<Obj*>(resume_fiber_));
    }
    // Mark signal/effect tracking
    if (current_effect_) {
        gc_mark_object(static_cast<Obj*>(current_effect_));
    }
    for (auto* eff : effect_queue_) {
        gc_mark_object(static_cast<Obj*>(eff));
    }
}

void VM::collect_garbage() {
    // Profiler: record GC start
    bool gc_profiled = profiler_.is_profiling();
    double gc_start = gc_profiled ? profiler_.now_us() : 0;
    size_t before_bytes = get_allocated_bytes();
    if (gc_profiled) profiler_.record_gc_start();

    // Mark roots from ALL active VMs
    for (VM* vm : active_vms_) {
        vm->mark_roots();
    }
    // Mark interned strings (they must survive GC or StringTable has dangling pointers)
    gc_mark_string_table();
    // Trace all references
    gc_drain_gray_stack();
    // Sweep unmarked objects
    gc_sweep();
    // Grow the threshold for next collection
    set_next_gc(get_allocated_bytes() * 2);
    gc_phase_ = GCPhase::Idle;

    // Profiler: record GC end
    if (gc_profiled) {
        double gc_dur = profiler_.now_us() - gc_start;
        size_t freed = (before_bytes > get_allocated_bytes()) ? (before_bytes - get_allocated_bytes()) : 0;
        profiler_.record_gc_end(gc_dur, freed);
    }
}

void VM::gc_step() {
    // Only start GC if threshold exceeded
    if (gc_phase_ == GCPhase::Idle && get_allocated_bytes() < get_next_gc()) return;

    switch (gc_phase_) {
        case GCPhase::Idle: {
            // Start marking: mark roots from ALL active VMs
            for (VM* vm : active_vms_) {
                vm->mark_roots();
            }
            gc_mark_string_table();
            gc_phase_ = GCPhase::Marking;
            // Fall through to do some marking work
            [[fallthrough]];
        }
        case GCPhase::Marking: {
            // Process a batch of gray objects (incremental)
            int work = 0;
            while (!gc_gray_stack_empty() && work < GC_MARK_WORK) {
                Obj* obj = gc_gray_stack_pop();
                gc_trace_references(obj);
                work++;
            }
            // If gray stack is empty, re-mark roots (objects allocated during
            // marking won't have been marked yet) and continue marking
            if (gc_gray_stack_empty()) {
                // Re-mark roots to catch objects allocated during incremental marking
                for (VM* vm : active_vms_) {
                    vm->mark_roots();
                }
                gc_mark_string_table();
                // If gray stack is still empty after re-marking, move to sweep
                if (gc_gray_stack_empty()) {
                    gc_phase_ = GCPhase::Sweeping;
                }
                // Otherwise continue marking next step
            }
            if (gc_phase_ != GCPhase::Sweeping) return;
            // Fall through to sweep
            [[fallthrough]];
        }
        case GCPhase::Sweeping: {
            // Sweep all unmarked objects atomically
            gc_sweep();
            gc_phase_ = GCPhase::Idle;
            set_next_gc(get_allocated_bytes() * 2);
            return;
        }
    }
}

InterpretResult VM::interpret(const std::string& source) {
    // Tokenize
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    // Parse
    Parser parser(tokens);
    ASTPtr ast;
    try {
        ast = parser.parse_program();
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return InterpretResult::CompileError;
    }

    // Compile
    CodeGenerator codegen;
    ObjFunction* func;
    try {
        func = codegen.compile(ast);
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return InterpretResult::CompileError;
    }

    // Run
    return run_function(func);
}

InterpretResult VM::run_function(ObjFunction* function) {
    auto* closure = allocate_closure(function);
    stack_[stack_top_++] = Value(static_cast<Obj*>(closure));

    if (!call(closure, 0, 0, 0)) {
        return InterpretResult::RuntimeError;
    }

    return run();
}

InterpretResult VM::run_bytecode(const std::vector<uint8_t>& bytecode, const std::vector<Value>& constants) {
    // Create a function from bytecode and run it
    auto* func = allocate_function();
    func->bytecode = bytecode;
    func->constants = constants;
    func->register_count = 16; // default
    return run_function(func);
}

InterpretResult VM::run() {
    CallFrame* frame = &frames_[frame_count_ - 1];
    int base = frame->base_register;
    uint8_t* ip = frame->ip;  // cached instruction pointer (synced back before call/return)
    int call_a = 0, call_b = 0; // shared between normal CALL and WIDE CALL

    // Helper: refresh frame, base, and ip after call/return
    #define REFRESH_FRAME() do { frame = &frames_[frame_count_ - 1]; base = frame->base_register; ip = frame->ip; } while(0)

    // Helper: stack slot access via cached base
    #define S(i) stack_[base + (i)]

    // Macro to return RuntimeError but allow try/catch to catch it
    #define RETURN_RUNTIME_ERROR do { \
        if (exception_caught_) { \
            exception_caught_ = false; \
            REFRESH_FRAME(); \
            goto loop_continue; \
        } \
        return InterpretResult::RuntimeError; \
    } while(0)

    // GC check counter (check every 1024 opcodes to keep overhead low)
    int opcode_counter = 0;
    // Fiber yield handling - saves state and unwinds (safety net)
    #define HANDLE_FIBER_YIELD() do { \
        if (yield_pending_ && active_fiber_) { \
            ObjFiber* fib = active_fiber_; \
            /* Sync frame->ip with the local ip before saving state */ \
            frame->ip = ip; \
            int save_start = frames_[fib->frame_base].callee_stack_pos; \
            fib->saved_stack.clear(); \
            for (int _i = save_start; _i < stack_top_; _i++) { \
                fib->saved_stack.push_back(stack_[_i]); \
            } \
            fib->saved_stack_top = (int)fib->saved_stack.size(); \
            fib->saved_frame_count = frame_count_ - fib->frame_base; \
            fib->saved_caller_stack_top = frames_[fib->frame_base].caller_stack_top; \
            fib->saved_frames.clear(); \
            for (int _i = fib->frame_base; _i < frame_count_; _i++) { \
                ObjFiber::SavedFrame sf; \
                sf.base_register = frames_[_i].base_register - save_start; \
                sf.return_register = frames_[_i].return_register; \
                sf.callee_stack_pos = frames_[_i].callee_stack_pos - save_start; \
                sf.caller_stack_top = frames_[_i].caller_stack_top; \
                sf.ip_offset = (int)(frames_[_i].ip - frames_[_i].closure->function->bytecode.data()); \
                if (_i == frame_count_ - 1) sf.ip_offset -= 4; \
                fib->saved_frames.push_back(sf); \
            } \
            fib->saved_open_upvalues = open_upvalues_; \
            open_upvalues_ = nullptr; \
            fib->state = ObjFiber::State::Suspended; \
            active_fiber_ = fib->parent; \
            fib->parent = nullptr; \
            /* Return the yield value to the caller (like RETURN opcode) */ \
            int callee_pos = frames_[fib->frame_base].callee_stack_pos; \
            int caller_top = frames_[fib->frame_base].caller_stack_top; \
            frame_count_ = fib->frame_base; \
            stack_top_ = std::max(caller_top, callee_pos + 1); \
            stack_[callee_pos] = yield_value_; \
            REFRESH_FRAME(); \
            yield_pending_ = false; \
            goto loop_continue; \
        } \
    } while(0)

    #define CHECK_MEMORY_LIMIT() do { \
        if (__builtin_expect(++opcode_counter >= 128, 0)) { \
            opcode_counter = 0; \
            if (__builtin_expect(profiler_.is_profiling(), 0)) { profiler_.record_opcodes(128); } \
            gc_step(); \
            if (memory_limit_exceeded()) { \
                runtime_error("Memory limit exceeded (%zu bytes)", get_memory_limit()); \
                RETURN_RUNTIME_ERROR; \
            } \
        } \
    } while(0)

    // Verbose logging macro - only prints when -v flag is set
    #define VLOG(...) do { if (verbose_) { fprintf(stderr, __VA_ARGS__); fflush(stderr); } } while(0)
    // Helper to get IP offset from bytecode start
    #define IP_OFFSET() (int)(ip - frame->closure->function->bytecode.data())

#ifdef __GNUC__
    // Computed goto dispatch table
    static const void* dispatch_table[] = {
        &&op_LOAD_CONST, &&op_LOAD_NIL, &&op_LOAD_TRUE, &&op_LOAD_FALSE,
        &&op_MOVE, &&op_GET_LOCAL, &&op_SET_LOCAL, &&op_GET_UPVALUE, &&op_SET_UPVALUE,
        &&op_GET_GLOBAL, &&op_SET_GLOBAL,
        &&op_ADD, &&op_SUB, &&op_MUL, &&op_DIV, &&op_MOD, &&op_NEG,
        &&op_EQ, &&op_NEQ, &&op_LT, &&op_LTE, &&op_GT, &&op_GTE, &&op_NOT,
        &&op_JMP, &&op_JMP_IF_FALSE, &&op_JMP_IF_TRUE,
        &&op_CALL, &&op_CLOSURE, &&op_CLOSE_UPVALUE, &&op_RETURN,
        &&op_NEW_ARRAY, &&op_NEW_MAP, &&op_GET_INDEX, &&op_SET_INDEX,
        &&op_GET_FIELD, &&op_SET_FIELD,
        &&op_NEW_CLASS, &&op_NEW_INSTANCE, &&op_GET_METHOD, &&op_INVOKE,
        &&op_NEW_RANGE, &&op_ITER_INIT, &&op_ITER_NEXT, &&op_ITER_DONE,
        &&op_PRINT, &&op_HALT, &&op_NOP,
        // Quickened opcodes
        &&op_ADD_NUM, &&op_SUB_NUM, &&op_MUL_NUM, &&op_DIV_NUM, &&op_MOD_NUM, &&op_ADD_STR,
        &&op_EQ_NUM, &&op_NEQ_NUM, &&op_LT_NUM, &&op_LTE_NUM, &&op_GT_NUM, &&op_GTE_NUM,
        // Fused opcodes
        &&op_MOD_EQ_ZERO,
        // Fiber/coroutine
        &&op_FIBER_YIELD, &&op_FIBER_RESUME,
        // Tail call, await, exception
        &&op_TAIL_CALL, &&op_AWAIT, &&op_THROW, &&op_TRY_BEGIN, &&op_TRY_END,
        // Wide prefix
        &&op_WIDE,
        // Signal & Effect
        &&op_SIGNAL_CREATE, &&op_SIGNAL_GET, &&op_SIGNAL_SET,
        &&op_EFFECT_CREATE, &&op_EFFECT_RUN,
        // Enum
        &&op_ENUM_CREATE, &&op_ENUM_VARIANT, &&op_ENUM_DATA_VARIANT,
        &&op_ENUM_GET, &&op_ENUM_IS,
    };

    #define DISPATCH() do { \
        if (__builtin_expect(!effect_queue_.empty(), 0)) { goto loop_continue; } \
        goto *dispatch_table[ip[0]]; \
    } while(0)

    #define CASE(op) op_##op

    // Operator overloading: try calling a magic method on the left operand's class
    // Usage: TRY_OP_OVERLOAD(result_reg, left_val, right_val, "__add")
    // If the left operand is an instance with the magic method, calls it and jumps to DISPATCH.
    // Otherwise falls through (does nothing).
    #define TRY_OP_OVERLOAD(res_reg, lhs, rhs, magic_name) do { \
        if ((lhs).is_instance()) { \
            ObjClass* inst_klass = (lhs).as_instance()->klass; \
            auto it = inst_klass->methods.find(magic_name); \
            if (it != inst_klass->methods.end() && it->second.is_closure()) { \
                ObjClosure* method = it->second.as_closure(); \
                int call_abs = base + (res_reg); \
                /* call() will store closure at call_abs, args at call_abs+1..N */ \
                /* arity=2: this + other. The compiler counts 'this' as arg 0. */ \
                stack_[call_abs + 1] = (lhs); \
                stack_[call_abs + 2] = (rhs); \
                frame->ip = ip; \
                if (!call(method, 2, (res_reg), call_abs)) { RETURN_RUNTIME_ERROR; } \
                REFRESH_FRAME(); \
                DISPATCH(); \
            } \
        } \
    } while(0)

    // Unary operator overloading: try calling a magic method on the operand's class
    #define TRY_UNARY_OP_OVERLOAD(res_reg, operand, magic_name) do { \
        if ((operand).is_instance()) { \
            ObjClass* inst_klass = (operand).as_instance()->klass; \
            auto it = inst_klass->methods.find(magic_name); \
            if (it != inst_klass->methods.end() && it->second.is_closure()) { \
                ObjClosure* method = it->second.as_closure(); \
                int call_abs = base + (res_reg); \
                stack_[call_abs + 1] = (operand); \
                frame->ip = ip; \
                if (!call(method, 1, (res_reg), call_abs)) { RETURN_RUNTIME_ERROR; } \
                REFRESH_FRAME(); \
                DISPATCH(); \
            } \
        } \
    } while(0)

    for (;;) {
    loop_continue:
        // Drain queued effects at each dispatch cycle
        if (__builtin_expect(!effect_queue_.empty(), 0)) {
            ObjEffect* eff = effect_queue_.front();
            effect_queue_.pop_front();
            eff->state = ObjEffect::State::Running;
            // Profiler: record effect re-run
            if (__builtin_expect(profiler_.is_profiling(), 0)) {
                profiler_.record_effect_run(eff->name.c_str(), true);
            }
            // Clear old dependencies
            for (auto* old_sig : eff->dependencies) {
                auto& subs = old_sig->subscribers;
                subs.erase(std::remove(subs.begin(), subs.end(), eff), subs.end());
            }
            eff->dependencies.clear();
            current_effect_ = eff;
            effect_frame_depth_ = frame_count_ + 1;
            frame->ip = ip;
            int save_top = stack_top_;
            stack_[stack_top_++] = Value(static_cast<Obj*>(eff->body));
            int cp = save_top;
            if (!call(eff->body, 0, cp, cp)) {
                current_effect_ = nullptr;
                eff->state = ObjEffect::State::Idle;
                RETURN_RUNTIME_ERROR;
            }
            REFRESH_FRAME();
        }
        DISPATCH();

    CASE(LOAD_CONST): {
        uint8_t a = ip[1];
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        auto& constants = frame->closure->function->constants;
        if (bx >= constants.size()) { runtime_error("Invalid constant index"); RETURN_RUNTIME_ERROR; }
        S(a) = constants[bx];
        DISPATCH();
    }
    CASE(LOAD_NIL): {
        uint8_t a = ip[1];
        ip += 4;
        S(a) = Value();
        DISPATCH();
    }
    CASE(LOAD_TRUE): {
        uint8_t a = ip[1];
        ip += 4;
        S(a) = Value(true);
        DISPATCH();
    }
    CASE(LOAD_FALSE): {
        uint8_t a = ip[1];
        ip += 4;
        S(a) = Value(false);
        DISPATCH();
    }
    CASE(MOVE): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        S(a) = S(b);
        DISPATCH();
    }
    CASE(GET_LOCAL): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        S(a) = stack_[base + b];
        DISPATCH();
    }
    CASE(SET_LOCAL): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        stack_[base + b] = S(a);
        DISPATCH();
    }
    CASE(GET_UPVALUE): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        if (b >= frame->closure->upvalues.size()) { runtime_error("Invalid upvalue index"); RETURN_RUNTIME_ERROR; }
        S(a) = *frame->closure->upvalues[b]->location;
        DISPATCH();
    }
    CASE(SET_UPVALUE): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        if (b >= frame->closure->upvalues.size()) { runtime_error("Invalid upvalue index"); RETURN_RUNTIME_ERROR; }
        *frame->closure->upvalues[b]->location = S(a);
        DISPATCH();
    }
    CASE(GET_GLOBAL): {
        uint8_t a = ip[1];
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        auto& constants = frame->closure->function->constants;
        if (bx >= constants.size() || !constants[bx].is_string()) {
            runtime_error("Invalid global name");
            RETURN_RUNTIME_ERROR;
        }
        ObjString* name = constants[bx].as_string();
        auto it = globals_.find(name);
        if (it == globals_.end()) {
            runtime_error("Undefined variable '%s'", name->value.c_str());
            RETURN_RUNTIME_ERROR;
        }
        S(a) = it->second;
        DISPATCH();
    }
    CASE(SET_GLOBAL): {
        uint8_t a = ip[1];
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        auto& constants = frame->closure->function->constants;
        if (bx >= constants.size() || !constants[bx].is_string()) {
            runtime_error("Invalid global name");
            RETURN_RUNTIME_ERROR;
        }
        ObjString* name = constants[bx].as_string();
        globals_[name] = S(a);
        DISPATCH();
    }
    CASE(ADD): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() + rc.get_number());
            *(ip - 4) = op_byte(Opcode::ADD_NUM); // quicken
        } else if (rb.is_string() && rc.is_string()) {
            auto* result = get_string_table().intern(
                rb.as_string()->value + rc.as_string()->value);
            S(a) = Value(static_cast<Obj*>(result));
            *(ip - 4) = op_byte(Opcode::ADD_STR); // quicken
        } else {
            TRY_OP_OVERLOAD(a, rb, rc, "__add");
            runtime_error("Operands must be two numbers or two strings");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(ADD_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(S(b).get_number() + S(c).get_number());
        DISPATCH();
    }
    CASE(ADD_STR): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        auto* result = get_string_table().intern(S(b).as_string()->value + S(c).as_string()->value);
        S(a) = Value(static_cast<Obj*>(result));
        DISPATCH();
    }
    CASE(SUB): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() - rc.get_number());
            *(ip - 4) = op_byte(Opcode::SUB_NUM);
        } else { TRY_OP_OVERLOAD(a, rb, rc, "__sub"); runtime_error("Operands must be numbers"); RETURN_RUNTIME_ERROR; }
        DISPATCH();
    }
    CASE(SUB_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(S(b).get_number() - S(c).get_number());
        DISPATCH();
    }
    CASE(MUL): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() * rc.get_number());
            *(ip - 4) = op_byte(Opcode::MUL_NUM);
        } else { TRY_OP_OVERLOAD(a, rb, rc, "__mul"); runtime_error("Operands must be numbers"); RETURN_RUNTIME_ERROR; }
        DISPATCH();
    }
    CASE(MUL_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(S(b).get_number() * S(c).get_number());
        DISPATCH();
    }
    CASE(DIV): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            double d = rc.get_number();
            if (__builtin_expect(d == 0, 0)) { runtime_error("Division by zero"); RETURN_RUNTIME_ERROR; }
            S(a) = Value(rb.get_number() / d);
            *(ip - 4) = op_byte(Opcode::DIV_NUM);
        } else { TRY_OP_OVERLOAD(a, rb, rc, "__div"); runtime_error("Operands must be numbers"); RETURN_RUNTIME_ERROR; }
        DISPATCH();
    }
    CASE(DIV_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        double divisor = S(c).get_number();
        if (__builtin_expect(divisor == 0, 0)) { runtime_error("Division by zero"); RETURN_RUNTIME_ERROR; }
        S(a) = Value(S(b).get_number() / divisor);
        DISPATCH();
    }
    CASE(MOD): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            double d = rc.get_number();
            if (__builtin_expect(d == 0, 0)) { runtime_error("Modulo by zero"); RETURN_RUNTIME_ERROR; }
            S(a) = Value(std::fmod(rb.get_number(), d));
            *(ip - 4) = op_byte(Opcode::MOD_NUM);
        } else { TRY_OP_OVERLOAD(a, rb, rc, "__mod"); runtime_error("Operands must be numbers"); RETURN_RUNTIME_ERROR; }
        DISPATCH();
    }
    CASE(MOD_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(std::fmod(S(b).get_number(), S(c).get_number()));
        DISPATCH();
    }
    CASE(NEG): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        Value& rb = S(b);
        if (!rb.is_number()) {
            TRY_UNARY_OP_OVERLOAD(a, rb, "__neg");
            runtime_error("Operand must be a number");
            RETURN_RUNTIME_ERROR;
        }
        S(a) = Value(-rb.get_number());
        DISPATCH();
    }
    CASE(EQ): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        // For instances, try __eq overloading before pointer comparison
        if (rb.is_instance() && rc.is_instance()) {
            ObjClass* inst_klass = rb.as_instance()->klass;
            auto it = inst_klass->methods.find("__eq");
            if (it != inst_klass->methods.end() && it->second.is_closure()) {
                ObjClosure* method = it->second.as_closure();
                int call_abs = base + a;
                stack_[call_abs + 1] = rb;
                stack_[call_abs + 2] = rc;
                frame->ip = ip;
                if (!call(method, 2, a, call_abs)) { RETURN_RUNTIME_ERROR; }
                REFRESH_FRAME();
                DISPATCH();
            }
        }
        S(a) = Value(rb == rc);
        DISPATCH();
    }
    CASE(EQ_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(S(b).get_number() == S(c).get_number());
        DISPATCH();
    }
    CASE(NEQ): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        S(a) = Value(rb != rc);
        DISPATCH();
    }
    CASE(NEQ_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(S(b).get_number() != S(c).get_number());
        DISPATCH();
    }
    CASE(LT): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() < rc.get_number());
        } else if (rb.is_string() && rc.is_string()) {
            S(a) = Value(rb.as_string()->value < rc.as_string()->value);
        } else { TRY_OP_OVERLOAD(a, rb, rc, "__lt"); runtime_error("Operands must be two numbers or two strings"); RETURN_RUNTIME_ERROR; }
        DISPATCH();
    }
    CASE(LT_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(S(b).get_number() < S(c).get_number());
        DISPATCH();
    }
    CASE(LTE): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() <= rc.get_number());
        } else if (rb.is_string() && rc.is_string()) {
            S(a) = Value(rb.as_string()->value <= rc.as_string()->value);
        } else { TRY_OP_OVERLOAD(a, rb, rc, "__le"); runtime_error("Operands must be two numbers or two strings"); RETURN_RUNTIME_ERROR; }
        DISPATCH();
    }
    CASE(LTE_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(S(b).get_number() <= S(c).get_number());
        DISPATCH();
    }
    CASE(GT): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() > rc.get_number());
        } else if (rb.is_string() && rc.is_string()) {
            S(a) = Value(rb.as_string()->value > rc.as_string()->value);
        } else { TRY_OP_OVERLOAD(a, rb, rc, "__gt"); runtime_error("Operands must be two numbers or two strings"); RETURN_RUNTIME_ERROR; }
        DISPATCH();
    }
    CASE(GT_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(S(b).get_number() > S(c).get_number());
        DISPATCH();
    }
    CASE(GTE): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        Value& rb = S(b); Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() >= rc.get_number());
        } else if (rb.is_string() && rc.is_string()) {
            S(a) = Value(rb.as_string()->value >= rc.as_string()->value);
        } else { TRY_OP_OVERLOAD(a, rb, rc, "__ge"); runtime_error("Operands must be two numbers or two strings"); RETURN_RUNTIME_ERROR; }
        DISPATCH();
    }
    CASE(GTE_NUM): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(S(b).get_number() >= S(c).get_number());
        DISPATCH();
    }
    CASE(MOD_EQ_ZERO): {
        uint8_t a = ip[1]; uint8_t b = ip[2]; uint8_t c = ip[3]; ip += 4;
        S(a) = Value(std::fmod(S(b).get_number(), S(c).get_number()) == 0.0);
        DISPATCH();
    }
    CASE(NOT): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        S(a) = Value(!S(b).is_truthy());
        DISPATCH();
    }
    CASE(JMP): {
        uint16_t bx = (ip[2] << 8) | ip[3];
        int16_t offset = static_cast<int16_t>(bx);
        ip += offset;
        if (offset < 0) { // backward jump = loop back-edge
            CHECK_MEMORY_LIMIT();
            if (yield_pending_) HANDLE_FIBER_YIELD();
        }
        DISPATCH();
    }
    CASE(JMP_IF_FALSE): {
        uint8_t a = ip[1];
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        // Fast path: check for false/nil directly (most common from comparisons)
        uint64_t bits = S(a).bits;
        if (__builtin_expect(bits == Value::FALSE_VAL || bits == Value::NIL_VAL, 0)) {
            ip += static_cast<int16_t>(bx);
        } else if (__builtin_expect((bits & Value::NAN_BASE) == Value::NAN_BASE && bits != Value::TRUE_VAL, 0)) {
            // Non-boolean object - use full is_truthy check
            if (!S(a).is_truthy()) {
                ip += static_cast<int16_t>(bx);
            }
        }
        DISPATCH();
    }
    CASE(JMP_IF_TRUE): {
        uint8_t a = ip[1];
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        if (S(a).is_truthy()) {
            int16_t offset = static_cast<int16_t>(bx);
            ip += offset;
        }
        DISPATCH();
    }
    CASE(CALL): {
        call_a = ip[1];
        call_b = ip[2];
        ip += 4;
    }
    // Shared entry point for WIDE CALL (call_a/call_b already set, IP already advanced)
    do_call: {
        int a = call_a;
        int b = call_b;

        if (skip_native_call_) {
            skip_native_call_ = false;
            S(a) = skip_native_result_;
            DISPATCH();
        }

        int arg_count = b;
        int callee_abs = base + a;
        Value callee = stack_[callee_abs];
        VLOG("[CALL] a=%d args=%d callee_abs=%d callee=%s\n", a, arg_count, callee_abs, callee.to_string().c_str());
        if (callee.is_closure()) {
            auto* closure = callee.as_closure();
            // Profiler: record function call
            if (__builtin_expect(profiler_.is_profiling(), 0)) {
                profiler_.record_call(closure->function->name.c_str());
            }
            // Inline fast path: non-varargs, correct arity
            if (frame_count_ >= MAX_FRAMES) { runtime_error("Stack overflow"); RETURN_RUNTIME_ERROR; }
            int fixed_arity = closure->function->arity;
            if (closure->function->has_varargs || arg_count != fixed_arity) {
                frame->ip = ip; if (!call(closure, arg_count, a, callee_abs)) { RETURN_RUNTIME_ERROR; }
                REFRESH_FRAME();
            } else {
                int args_abs = callee_abs + 1;
                frame->ip = ip;
                CallFrame* new_frame = &frames_[frame_count_++];
                new_frame->closure = closure;
                new_frame->ip = closure->function->bytecode.data();
                new_frame->base_register = args_abs;
                new_frame->return_register = a;
                new_frame->callee_stack_pos = callee_abs;
                new_frame->caller_stack_top = stack_top_;
                new_frame->slots = &stack_[args_abs];
                int needed = closure->function->register_count;
                // Fast nil initialization using direct bit pattern write
                static constexpr uint64_t NIL_BITS = 0x7FFC000000000000ULL;
                for (int i = arg_count; i < needed; i++) {
                    stack_[args_abs + i].bits = NIL_BITS;
                }
                int new_top = args_abs + needed;
                if (new_top > stack_top_) stack_top_ = new_top;
                frame = new_frame;
                base = args_abs;
                ip = new_frame->ip;
            }
        } else if (callee.is_native()) {
            // Special case: fiber_yield - handle directly without calling native
            if (active_fiber_ && callee.as_native()->name == "fiber_yield") {
                ObjFiber* fib = active_fiber_;
                // Get the yield value from the first argument
                Value yval = (arg_count >= 1) ? stack_[callee_abs + 1] : Value();
                VLOG("[FIBER_YIELD] value=%s a=%d base=%d stack_top=%d frames=%d frame_base=%d\n", yval.to_string().c_str(), a, base, stack_top_, frame_count_, fib->frame_base);
                // Sync frame->ip with the local ip before saving state
                frame->ip = ip;
                // Save the fiber's own stack and frames (from the fiber's entry callee position)
                int save_start = frames_[fib->frame_base].callee_stack_pos;
                fib->saved_stack.clear();
                for (int _i = save_start; _i < stack_top_; _i++) {
                    fib->saved_stack.push_back(stack_[_i]);
                }
                fib->saved_stack_top = (int)fib->saved_stack.size();
                fib->saved_frame_count = frame_count_ - fib->frame_base;
                fib->saved_caller_stack_top = frames_[fib->frame_base].caller_stack_top;
                fib->saved_frames.clear();
                for (int _i = fib->frame_base; _i < frame_count_; _i++) {
                    ObjFiber::SavedFrame sf;
                    sf.base_register = frames_[_i].base_register - save_start;
                    sf.return_register = frames_[_i].return_register;
                    sf.callee_stack_pos = frames_[_i].callee_stack_pos - save_start;
                    sf.caller_stack_top = frames_[_i].caller_stack_top;
                    // IP points past the CALL (already advanced). On resume,
                    // execution continues from the same CALL instruction.
                    sf.ip_offset = (int)(frames_[_i].ip - frames_[_i].closure->function->bytecode.data());
                    if (_i == frame_count_ - 1) sf.ip_offset -= 4;
                    fib->saved_frames.push_back(sf);
                }
                fib->saved_open_upvalues = open_upvalues_;
                open_upvalues_ = nullptr;
                fib->state = ObjFiber::State::Suspended;
                active_fiber_ = fib->parent;
                fib->parent = nullptr;
                // "Return" the yield value to the caller (like RETURN opcode)
                int callee_pos = frames_[fib->frame_base].callee_stack_pos;
                int caller_top = frames_[fib->frame_base].caller_stack_top;
                frame_count_ = fib->frame_base;
                stack_top_ = std::max(caller_top, callee_pos + 1);
                stack_[callee_pos] = yval;
                REFRESH_FRAME();
                yield_pending_ = false;
                DISPATCH();
            }
            if (!call_native(callee.as_native(), arg_count, a, callee_abs)) {
                RETURN_RUNTIME_ERROR;
            }
            // Handle fiber yield after other native calls (e.g. from a native that calls fiber_yield)
            if (yield_pending_ && active_fiber_) {
                // This shouldn't happen normally since fiber_yield is handled above
                yield_pending_ = false;
            }
            // Check for deferred fiber resume (set by native fiber_resume)
            if (resume_pending_) {
                resume_pending_ = false;
                // Save caller's IP before switching to fiber frame
                frame->ip = ip;
                ObjFiber* fib = resume_fiber_;
                resume_fiber_ = nullptr;
                resume_return_reg_ = a; // save the return register from the CALL instruction
                // Reset stack to before the native call (clear the placeholder nil)
                stack_top_ = callee_abs;
                VLOG("[FIBER_RESUME] state=%d saved_stack=%zu saved_frames=%d\n", (int)fib->state, fib->saved_stack.size(), fib->saved_frame_count);
                if (fib->state == ObjFiber::State::Suspended) {
                    // Restore saved state from fiber
                    // Use callee_abs as restore base (where the parent pushed the callee)
                    int restore_base = callee_abs;
                    stack_top_ = restore_base;
                    for (size_t i = 0; i < fib->saved_stack.size(); i++) {
                        stack_[stack_top_++] = fib->saved_stack[i];
                    }
                    int new_frame_base = frame_count_;
                    frame_count_ = new_frame_base + fib->saved_frame_count;
                    for (int i = 0; i < fib->saved_frame_count; i++) {
                        auto& sf = fib->saved_frames[i];
                        auto& f = frames_[new_frame_base + i];
                        f.base_register = sf.base_register + restore_base;
                        f.return_register = sf.return_register;
                        f.callee_stack_pos = sf.callee_stack_pos + restore_base;
                        f.caller_stack_top = sf.caller_stack_top;
                        int closure_idx = sf.callee_stack_pos;
                        if (closure_idx >= 0 && closure_idx < (int)fib->saved_stack.size() && fib->saved_stack[closure_idx].is_closure()) {
                            f.closure = fib->saved_stack[closure_idx].as_closure();
                            f.ip = f.closure->function->bytecode.data() + sf.ip_offset;
                        }
                        VLOG("[FIBER_RESUME] frame[%d] base=%d callee_pos=%d ip_offset=%d\n", i, f.base_register, f.callee_stack_pos, sf.ip_offset);
                    }
                    fib->frame_base = new_frame_base;
                    // Update entry frame's callee_stack_pos so RETURN stores result at callee_abs
                    frames_[new_frame_base].callee_stack_pos = callee_abs;
                    open_upvalues_ = fib->saved_open_upvalues;
                    fib->state = ObjFiber::State::Running;
                    fib->parent = active_fiber_;
                    fib->resume_return_reg = resume_return_reg_;
                    active_fiber_ = fib;

                    resume_has_value_ = false;
                    // The IP points to the CALL instruction. The script will
                    // re-enter the CALL handler. Set skip_native_call_ so the native
                    // isn't called again. Place the resume value at callee_abs
                    // (this is what `let step = fiber_yield(x)` receives as `step`).
                    skip_native_call_ = true;
                    skip_native_result_ = resume_value_;
                    REFRESH_FRAME();
                } else if (fib->state == ObjFiber::State::Created) {
                    // Start the fiber for the first time
                    fib->state = ObjFiber::State::Running;
                    fib->parent = active_fiber_;
                    fib->resume_return_reg = resume_return_reg_;
                    active_fiber_ = fib;
                    int callee_abs2 = stack_top_;
                    stack_[stack_top_++] = Value(static_cast<Obj*>(fib->entry));
                    // Use resume value if provided (and non-nil), otherwise use initial_args
                    int arg_count = 0;
                    if (resume_has_value_ && !resume_value_.is_nil()) {
                        stack_[stack_top_++] = resume_value_;
                        arg_count = 1;
                    } else {
                        arg_count = (int)fib->initial_args.size();
                        for (int i = 0; i < arg_count; i++) {
                            stack_[stack_top_++] = fib->initial_args[i];
                        }
                    }
                    resume_has_value_ = false;
                    fib->frame_base = frame_count_; // set before call() increments frame_count_
                    frame->ip = ip; if (!call(fib->entry, arg_count, resume_return_reg_, callee_abs2)) {
                        active_fiber_ = nullptr;
                        RETURN_RUNTIME_ERROR;
                    }
                    REFRESH_FRAME();
                }
            }
        } else if (callee.is_class()) {
            auto* instance = allocate_instance(callee.as_class());
            Value instance_val = Value(static_cast<Obj*>(instance));
            S(a) = instance_val;
            auto& methods = callee.as_class()->methods;
            auto init_it = methods.find("init");
            if (init_it == methods.end()) init_it = methods.find(HASH_INIT);
            if (init_it != methods.end() && init_it->second.is_closure()) {
                auto* init_closure = init_it->second.as_closure();
                int total_args = arg_count + 1;
                if (total_args != init_closure->function->arity) {
                    runtime_error("Expected %d arguments but got %d", init_closure->function->arity, total_args);
                    RETURN_RUNTIME_ERROR;
                }
                for (int i = arg_count; i > 0; i--) {
                    stack_[callee_abs + 1 + i] = stack_[callee_abs + 1 + i - 1];
                }
                stack_[callee_abs + 1] = instance_val;
                stack_top_ += 1;
                frame->ip = ip; if (!call(init_closure, total_args, a, callee_abs)) {
                    RETURN_RUNTIME_ERROR;
                }
                REFRESH_FRAME();
            } else {
                stack_[callee_abs] = instance_val;
            }
        } else {
            runtime_error("Can only call functions and classes");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(TAIL_CALL): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        int arg_count = b;
        int callee_abs = base + a;
        Value callee = stack_[callee_abs];

        if (callee.is_closure()) {
            auto* closure = callee.as_closure();
            if (arg_count != closure->function->arity) {
                runtime_error("Expected %d arguments but got %d", closure->function->arity, arg_count);
                RETURN_RUNTIME_ERROR;
            }
            while (open_upvalues_ && open_upvalues_->location >= &stack_[base]) {
                ObjUpvalue* uv = open_upvalues_;
                uv->closed = *uv->location;
                uv->location = &uv->closed;
                open_upvalues_ = uv->next_upvalue;
            }
            int args_src = callee_abs + 1;
            for (int i = 0; i < arg_count; i++) {
                stack_[base + i] = stack_[args_src + i];
            }
            int needed = closure->function->register_count;
            for (int i = arg_count; i < needed; i++) {
                stack_[base + i] = Value();
            }
            frame->closure = closure;
            ip = closure->function->bytecode.data();
            stack_top_ = base + needed;
        } else if (callee.is_native()) {
            auto* native = callee.as_native();
            Value* args = &stack_[callee_abs + 1];
            Value result = native->function(arg_count, args);
            int callee_pos = frame->callee_stack_pos;
            frame_count_--;
            if (frame_count_ == 0) {
                stack_top_ = 0;
                stack_[stack_top_++] = result;
                return InterpretResult::Ok;
            }
            stack_top_ = callee_pos;
            stack_[stack_top_++] = result;
            REFRESH_FRAME();
        } else if (callee.is_class()) {
            auto* instance = allocate_instance(callee.as_class());
            Value instance_val = Value(static_cast<Obj*>(instance));
            stack_[callee_abs] = instance_val;
            auto& methods = callee.as_class()->methods;
            auto init_it = methods.find("init");
            if (init_it == methods.end()) init_it = methods.find(HASH_INIT);
            if (init_it != methods.end() && init_it->second.is_closure()) {
                auto* init_closure = init_it->second.as_closure();
                int total_args = arg_count + 1;
                if (total_args != init_closure->function->arity) {
                    runtime_error("Expected %d arguments but got %d", init_closure->function->arity, total_args);
                    RETURN_RUNTIME_ERROR;
                }
                for (int i = arg_count; i > 0; i--) {
                    stack_[callee_abs + 1 + i] = stack_[callee_abs + 1 + i - 1];
                }
                stack_[callee_abs + 1] = instance_val;
                stack_top_ += 1;
                while (open_upvalues_ && open_upvalues_->location >= &stack_[base]) {
                    ObjUpvalue* uv = open_upvalues_;
                    uv->closed = *uv->location;
                    uv->location = &uv->closed;
                    open_upvalues_ = uv->next_upvalue;
                }
                int args_src = callee_abs + 1;
                for (int i = 0; i < total_args; i++) {
                    stack_[base + i] = stack_[args_src + i];
                }
                int needed = init_closure->function->register_count;
                for (int i = total_args; i < needed; i++) {
                    stack_[base + i] = Value();
                }
                frame->closure = init_closure;
                ip = init_closure->function->bytecode.data();
                stack_top_ = base + needed;
            }
        } else {
            runtime_error("Tail call target must be a function or class");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(CLOSURE): {
        uint8_t a = ip[1];
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        auto& constants = frame->closure->function->constants;
        if (bx >= constants.size() || !constants[bx].is_obj() ||
            constants[bx].as_obj()->type != ObjType::Function) {
            runtime_error("Invalid closure constant");
            RETURN_RUNTIME_ERROR;
        }
        auto* func = static_cast<ObjFunction*>(constants[bx].as_obj());
        auto* closure = allocate_closure(func);
        for (auto& desc : func->upvalue_descs) {
            ObjUpvalue* upvalue = nullptr;
            if (desc.is_local) {
                Value* local_slot = &stack_[base + desc.index];
                ObjUpvalue** pp = &open_upvalues_;
                while (*pp && (*pp)->location > local_slot) {
                    pp = &(*pp)->next_upvalue;
                }
                if (*pp && (*pp)->location == local_slot) {
                    upvalue = *pp;
                } else {
                    upvalue = allocate_upvalue(local_slot);
                    upvalue->next_upvalue = *pp;
                    *pp = upvalue;
                }
            } else {
                upvalue = frame->closure->upvalues[desc.index];
            }
            closure->upvalues.push_back(upvalue);
        }
        S(a) = Value(static_cast<Obj*>(closure));
        DISPATCH();
    }
    CASE(CLOSE_UPVALUE): {
        uint8_t a = ip[1];
        ip += 4;
        Value* slot_ptr = &stack_[base + a];
        while (open_upvalues_ && open_upvalues_->location >= slot_ptr) {
            ObjUpvalue* uv = open_upvalues_;
            uv->closed = *uv->location;
            uv->location = &uv->closed;
            open_upvalues_ = uv->next_upvalue;
        }
        DISPATCH();
    }
    CASE(RETURN): {
        uint8_t a = ip[1];
        ip += 4;
        Value result = S(a);
        int callee_pos = frame->callee_stack_pos;
        int saved_top = frame->caller_stack_top;
        // Profiler: record function return BEFORE popping the frame
        if (__builtin_expect(profiler_.is_profiling(), 0)) {
            const char* fname = frame->closure->function->name.c_str();
            profiler_.record_return(fname, 0);
        }
        frame_count_--;
        // Clear effect context only when returning from the effect's own frame
        // (not from helper functions called within the effect)
        if (__builtin_expect(current_effect_ != nullptr && frame_count_ < effect_frame_depth_, 0)) {
            current_effect_->state = ObjEffect::State::Idle;
            current_effect_ = nullptr;
        }
        if (__builtin_expect(frame_count_ == 0, 0)) {
            stack_top_ = 0;
            stack_[stack_top_++] = result;
            return InterpretResult::Ok;
        }
        // If this was the last fiber frame
        if (__builtin_expect(active_fiber_ && frame_count_ == active_fiber_->frame_base, 0)) {
            active_fiber_->state = ObjFiber::State::Done;
            active_fiber_ = active_fiber_->parent;
        }
        // Restore caller's stack_top and result
        stack_top_ = std::max(saved_top, callee_pos + 1);
        stack_[callee_pos] = result;
        // Inline REFRESH_FRAME for speed
        frame = &frames_[frame_count_ - 1];
        base = frame->base_register;
        ip = frame->ip;
        // If effects were queued, go back to loop_continue to drain them
        if (__builtin_expect(!effect_queue_.empty(), 0)) goto loop_continue;
        DISPATCH();
    }
    CASE(AWAIT): {
        uint8_t a = ip[1];
        ip += 4;
        Value val = S(a);
        if (val.is_nil()) {
            int callee_pos = frame->callee_stack_pos;
            int saved_top = frame->caller_stack_top;
            frame_count_--;
            if (frame_count_ == 0) {
                stack_top_ = 0;
                stack_[stack_top_++] = Value();
                return InterpretResult::Ok;
            }
            stack_top_ = std::max(saved_top, callee_pos + 1);
            stack_[callee_pos] = Value();
            REFRESH_FRAME();
            DISPATCH();
        }
        DISPATCH();
    }
    CASE(THROW): {
        uint8_t a = ip[1];
        ip += 4;
        Value exception = S(a);
        std::string msg = exception.to_string();
        if (try_count_ > 0) {
            TryFrame& tf = try_frames_[--try_count_];
            while (frame_count_ > tf.frame_count) {
                // Close upvalues at this frame's base before unwinding
                Value* slot_ptr = &stack_[frames_[frame_count_ - 1].base_register];
                while (open_upvalues_ && open_upvalues_->location >= slot_ptr) {
                    ObjUpvalue* uv = open_upvalues_;
                    uv->closed = *uv->location;
                    uv->location = &uv->closed;
                    open_upvalues_ = uv->next_upvalue;
                }
                frame_count_--;
            }
            REFRESH_FRAME();
            ip = tf.catch_ip;
            if (exception.is_string()) {
                S(tf.catch_register) = exception;
            } else {
                auto* err_str = get_string_table().intern(msg);
                S(tf.catch_register) = Value(static_cast<Obj*>(err_str));
            }
            DISPATCH();
        }
        runtime_error("%s", msg.c_str());
        RETURN_RUNTIME_ERROR;
    }
    CASE(TRY_BEGIN): {
        uint8_t a = ip[1];
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        if (try_count_ >= MAX_TRY) {
            runtime_error("Too many nested try blocks");
            RETURN_RUNTIME_ERROR;
        }
        TryFrame& tf = try_frames_[try_count_++];
        tf.frame_count = frame_count_;
        tf.catch_ip = ip + static_cast<int16_t>(bx);
        tf.catch_register = a;
        DISPATCH();
    }
    CASE(TRY_END): {
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        if (try_count_ > 0) try_count_--;
        if (bx > 0) {
            ip += bx;
        }
        DISPATCH();
    }
    CASE(NEW_ARRAY): {
        uint8_t a = ip[1];
        ip += 4;
        auto* arr = allocate_array();
        S(a) = Value(static_cast<Obj*>(arr));
        DISPATCH();
    }
    CASE(NEW_MAP): {
        uint8_t a = ip[1];
        ip += 4;
        auto* map = allocate_map();
        S(a) = Value(static_cast<Obj*>(map));
        DISPATCH();
    }
    CASE(GET_INDEX): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        Value obj = S(b);
        Value idx = S(c);
        if (obj.is_array()) {
            if (!idx.is_number()) {
                runtime_error("Array index must be a number");
                RETURN_RUNTIME_ERROR;
            }
            auto& elems = obj.as_array()->elements;
            int i = static_cast<int>(idx.get_number());
            if (i < 0) i += (int)elems.size();
            if (i < 0 || i >= (int)elems.size()) {
                runtime_error("Array index out of bounds");
                RETURN_RUNTIME_ERROR;
            }
            S(a) = elems[i];
        } else if (obj.is_map()) {
            if (!idx.is_string()) {
                runtime_error("Map key must be a string");
                RETURN_RUNTIME_ERROR;
            }
            auto& entries = obj.as_map()->entries;
            auto it = entries.find(idx.as_string()->value);
            S(a) = (it == entries.end()) ? Value() : it->second;
        } else if (obj.is_string()) {
            if (!idx.is_number()) {
                runtime_error("String index must be a number");
                RETURN_RUNTIME_ERROR;
            }
            auto& s = obj.as_string()->value;
            int i = static_cast<int>(idx.get_number());
            if (i < 0) i += (int)s.size();
            if (i < 0 || i >= (int)s.size()) {
                runtime_error("String index out of bounds");
                RETURN_RUNTIME_ERROR;
            }
            auto* ch = get_string_table().intern(std::string(1, s[i]));
            S(a) = Value(static_cast<Obj*>(ch));
        } else {
            runtime_error("Cannot index this value");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(SET_INDEX): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        Value obj = S(a);
        Value idx = S(b);
        Value val = S(c);
        if (obj.is_array()) {
            if (!idx.is_number()) {
                runtime_error("Array index must be a number");
                RETURN_RUNTIME_ERROR;
            }
            auto& elems = obj.as_array()->elements;
            int i = static_cast<int>(idx.get_number());
            if (i < 0) i += (int)elems.size();
            if (i < 0) {
                runtime_error("Array index out of bounds");
                RETURN_RUNTIME_ERROR;
            }
            if (i >= (int)elems.size()) elems.resize(i + 1);
            elems[i] = val;
        } else if (obj.is_map()) {
            if (!idx.is_string()) {
                runtime_error("Map key must be a string");
                RETURN_RUNTIME_ERROR;
            }
            obj.as_map()->entries[idx.as_string()->value] = val;
        } else {
            runtime_error("Cannot index this value");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(GET_FIELD): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        Value obj = S(b);
        auto& constants = frame->closure->function->constants;
        if (c >= constants.size() || !constants[c].is_string()) {
            runtime_error("Invalid field name");
            RETURN_RUNTIME_ERROR;
        }
        const std::string& field = constants[c].as_string()->value;
        if (obj.is_instance()) {
            auto& fields = obj.as_instance()->fields;
            auto it = fields.find(field);
            if (it != fields.end()) {
                S(a) = it->second;
            } else {
                auto& methods = obj.as_instance()->klass->methods;
                auto meth_it = methods.find(field);
                S(a) = (meth_it != methods.end()) ? meth_it->second : Value();
            }
        } else if (obj.is_map()) {
            auto it = obj.as_map()->entries.find(field);
            S(a) = (it != obj.as_map()->entries.end()) ? it->second : Value();
        } else if (obj.is_class()) {
            auto& methods = obj.as_class()->methods;
            auto it = methods.find(field);
            S(a) = (it != methods.end()) ? it->second : Value();
        } else if (obj.is_string()) {
            if (field == "length" || field == HASH_LENGTH) {
                S(a) = Value(static_cast<double>(obj.as_string()->value.size()));
            } else {
                S(a) = Value();
            }
        } else if (obj.is_array()) {
            if (field == "length" || field == HASH_LENGTH) {
                S(a) = Value(static_cast<double>(obj.as_array()->elements.size()));
            } else {
                S(a) = Value();
            }
        } else {
            S(a) = Value();
        }
        DISPATCH();
    }
    CASE(SET_FIELD): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        Value obj = S(a);
        auto& constants = frame->closure->function->constants;
        if (b >= constants.size() || !constants[b].is_string()) {
            runtime_error("Invalid field name");
            RETURN_RUNTIME_ERROR;
        }
        const std::string& field = constants[b].as_string()->value;
        if (obj.is_instance()) {
            obj.as_instance()->fields[field] = S(c);
        } else if (obj.is_class()) {
            obj.as_class()->methods[field] = S(c);
        } else if (obj.is_map()) {
            obj.as_map()->entries[field] = S(c);
        } else {
            runtime_error("Cannot set field on this value");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(NEW_CLASS): {
        uint8_t a = ip[1];
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        auto& constants = frame->closure->function->constants;
        if (bx >= constants.size() || !constants[bx].is_string()) {
            runtime_error("Invalid class name");
            RETURN_RUNTIME_ERROR;
        }
        auto* cls = allocate_class(constants[bx].as_string()->value);
        S(a) = Value(static_cast<Obj*>(cls));
        DISPATCH();
    }
    CASE(NEW_INSTANCE): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        if (!S(b).is_class()) {
            runtime_error("Cannot instantiate non-class");
            RETURN_RUNTIME_ERROR;
        }
        auto* inst = allocate_instance(S(b).as_class());
        S(a) = Value(static_cast<Obj*>(inst));
        DISPATCH();
    }
    CASE(GET_METHOD): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        Value obj = S(b);
        auto& constants = frame->closure->function->constants;
        if (c >= constants.size() || !constants[c].is_string()) {
            S(a) = Value();
            DISPATCH();
        }
        std::string method = constants[c].as_string()->value;
        if (obj.is_instance()) {
            auto& methods = obj.as_instance()->klass->methods;
            auto it = methods.find(method);
            S(a) = (it != methods.end()) ? it->second : Value();
        } else {
            S(a) = Value();
        }
        DISPATCH();
    }
    CASE(INVOKE): {
        runtime_error("INVOKE not fully implemented");
        RETURN_RUNTIME_ERROR;
    }
    CASE(NEW_RANGE): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        if (!S(b).is_number() || !S(c).is_number()) {
            runtime_error("Range bounds must be numbers");
            RETURN_RUNTIME_ERROR;
        }
        auto* map = allocate_map();
        map->entries["start"] = S(b);
        map->entries["end"] = S(c);
        map->entries["__type__"] = Value(static_cast<Obj*>(get_string_table().intern("range")));
        S(a) = Value(static_cast<Obj*>(map));
        DISPATCH();
    }
    CASE(ITER_INIT): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        Value iterable = S(b);
        auto* iter = allocate_iterator();
        if (iterable.is_array()) {
            iter->kind = ObjIterator::ArrayIter;
            iter->arr = iterable.as_array();
            iter->arr_index = 0;
        } else if (iterable.is_map() && iterable.as_map()->entries.count("__type__") &&
                   iterable.as_map()->entries["__type__"].is_string() &&
                   iterable.as_map()->entries["__type__"].as_string()->value == "range") {
            double start_val = iterable.as_map()->entries["start"].get_number();
            double end_val = iterable.as_map()->entries["end"].get_number();
            iter->kind = ObjIterator::RangeIter;
            iter->range_current = start_val;
            iter->range_end = end_val;
            iter->range_step = start_val <= end_val ? 1.0 : -1.0;
        } else if (iterable.is_string()) {
            iter->kind = ObjIterator::StringIter;
            iter->str = iterable.as_string();
            iter->str_index = 0;
        } else {
            runtime_error("Cannot iterate this value");
            RETURN_RUNTIME_ERROR;
        }
        S(a) = Value(static_cast<Obj*>(iter));
        DISPATCH();
    }
    CASE(ITER_NEXT): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        Value iter_val = S(b);
        if (!iter_val.is_iterator()) {
            runtime_error("Invalid iterator");
            RETURN_RUNTIME_ERROR;
        }
        auto* iter = iter_val.as_iterator();
        if (iter->kind == ObjIterator::ArrayIter) {
            if (iter->arr_index < (int)iter->arr->elements.size()) {
                S(a) = iter->arr->elements[iter->arr_index++];
            } else {
                iter->done = true;
                S(a) = Value();
            }
        } else if (iter->kind == ObjIterator::RangeIter) {
            bool done = (iter->range_step > 0) ? (iter->range_current > iter->range_end) : (iter->range_current < iter->range_end);
            if (!done) {
                S(a) = Value(iter->range_current);
                iter->range_current += iter->range_step;
            } else {
                iter->done = true;
                S(a) = Value();
            }
        } else { // StringIter
            auto& s = iter->str->value;
            if (iter->str_index < (int)s.size()) {
                auto* ch = get_string_table().intern(std::string(1, s[iter->str_index++]));
                S(a) = Value(static_cast<Obj*>(ch));
            } else {
                iter->done = true;
                S(a) = Value();
            }
        }
        DISPATCH();
    }
    CASE(ITER_DONE): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        Value iter = S(b);
        if (iter.is_iterator()) {
            S(a) = Value(iter.as_iterator()->done);
        } else {
            S(a) = Value(true);
        }
        DISPATCH();
    }
    CASE(PRINT): {
        uint8_t a = ip[1];
        ip += 4;
        std::cout << S(a).to_string() << std::endl;
        DISPATCH();
    }
    CASE(HALT):
        return InterpretResult::Ok;
    CASE(NOP):
        ip += 4;
        DISPATCH();
    CASE(FIBER_YIELD): {
        uint8_t a = ip[1];
        ip += 4;
        if (skip_native_call_) {
            skip_native_call_ = false;
            S(a) = skip_native_result_;
            DISPATCH();
        }
        if (!active_fiber_) {
            runtime_error("Cannot yield outside a fiber");
            RETURN_RUNTIME_ERROR;
        }
        yield_pending_ = true;
        yield_value_ = S(a);
        DISPATCH(); // next DISPATCH will handle the actual yield via HANDLE_FIBER_YIELD
    }
    CASE(FIBER_RESUME): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        Value fiber_val = S(b);
        Value resume_val = S(c);
        if (!fiber_val.is_fiber()) {
            runtime_error("Can only resume a fiber");
            RETURN_RUNTIME_ERROR;
        }
        auto* fiber = fiber_val.as_fiber();
        if (fiber->state == ObjFiber::State::Done) {
            runtime_error("Cannot resume a finished fiber");
            RETURN_RUNTIME_ERROR;
        }
        if (fiber->state == ObjFiber::State::Suspended) {
            // Restore saved state from fiber into VM
            int caller_base = frame->base_register;
            // Restore the fiber's stack region with bounds check
            stack_top_ = caller_base;
            if (stack_top_ + (int)fiber->saved_stack.size() > MAX_STACK) {
                runtime_error("Stack overflow resuming fiber");
                RETURN_RUNTIME_ERROR;
            }
            for (size_t i = 0; i < fiber->saved_stack.size(); i++) {
                stack_[stack_top_++] = fiber->saved_stack[i];
            }
            // Restore frames at the current frame_count_ (after caller's frames)
            int new_frame_base = frame_count_;
            if (new_frame_base + fiber->saved_frame_count > MAX_FRAMES) {
                runtime_error("Frame overflow resuming fiber");
                RETURN_RUNTIME_ERROR;
            }
            frame_count_ = new_frame_base + fiber->saved_frame_count;
            for (int i = 0; i < fiber->saved_frame_count; i++) {
                auto& sf = fiber->saved_frames[i];
                auto& f = frames_[new_frame_base + i];
                f.base_register = sf.base_register + caller_base;
                f.return_register = sf.return_register;
                f.callee_stack_pos = sf.callee_stack_pos + caller_base;
                f.caller_stack_top = sf.caller_stack_top;
                // sf.callee_stack_pos is already an offset from save_start (i.e., into saved_stack)
                if (sf.callee_stack_pos >= 0 && sf.callee_stack_pos < (int)fiber->saved_stack.size() && fiber->saved_stack[sf.callee_stack_pos].is_closure()) {
                    f.closure = fiber->saved_stack[sf.callee_stack_pos].as_closure();
                    f.ip = f.closure->function->bytecode.data() + sf.ip_offset;
                }
            }
            fiber->frame_base = new_frame_base;
            open_upvalues_ = fiber->saved_open_upvalues;
            fiber->state = ObjFiber::State::Running;
            fiber->parent = active_fiber_;
            fiber->resume_return_reg = a;
            active_fiber_ = fiber;

            // Set up resume value to be returned by the yield call
            skip_native_call_ = true;
            skip_native_result_ = resume_val;

            REFRESH_FRAME();
            DISPATCH();
        }
        // Created state: start the fiber
        fiber->state = ObjFiber::State::Running;
        fiber->parent = active_fiber_;
        fiber->resume_return_reg = a;
        active_fiber_ = fiber;
        int callee_abs = stack_top_;
        stack_[stack_top_++] = Value(static_cast<Obj*>(fiber->entry));
        // Use resume value if provided
        int arg_count = 0;
        if (!resume_val.is_nil()) {
            stack_[stack_top_++] = resume_val;
            arg_count = 1;
        } else {
            arg_count = (int)fiber->initial_args.size();
            for (int i = 0; i < arg_count; i++) {
                stack_[stack_top_++] = fiber->initial_args[i];
            }
        }
        fiber->frame_base = frame_count_; // set before call() increments frame_count_
        frame->ip = ip; if (!call(fiber->entry, arg_count, a, callee_abs)) {
            active_fiber_ = nullptr;
            RETURN_RUNTIME_ERROR;
        }
        REFRESH_FRAME();
        DISPATCH();
    }

    CASE(WIDE): {
        // Wide instruction: [WIDE:8][op:8][A:16][B:16][C:16] = 8 bytes total
        uint8_t* wip = ip;
        uint8_t wide_op = wip[1];
        uint16_t wa = (wip[2] << 8) | wip[3];
        uint16_t wb = (wip[4] << 8) | wip[5];
        uint16_t wc = (wip[6] << 8) | wip[7];
        VLOG("[WIDE] op=%d a=%d b=%d c=%d ip_offset=%d\n", wide_op, wa, wb, wc, (int)(wip - frame->closure->function->bytecode.data()));
        ip += WIDE_INST_SIZE;

        switch (static_cast<Opcode>(wide_op)) {
            case Opcode::LOAD_CONST: {
                auto& constants = frame->closure->function->constants;
                uint16_t idx = (wb << 8) | wc;
                if (idx >= constants.size()) { runtime_error("Invalid constant index"); RETURN_RUNTIME_ERROR; }
                S(wa) = constants[idx];
                break;
            }
            case Opcode::LOAD_NIL: S(wa) = Value(); break;
            case Opcode::LOAD_TRUE: S(wa) = Value(true); break;
            case Opcode::LOAD_FALSE: S(wa) = Value(false); break;
            case Opcode::MOVE: S(wa) = S(wb); break;
            case Opcode::GET_LOCAL: S(wa) = stack_[frame->base_register + wb]; break;
            case Opcode::GET_GLOBAL: {
                uint16_t bx = (wb << 8) | wc;
                auto& constants = frame->closure->function->constants;
                if (bx >= constants.size() || !constants[bx].is_string()) {
                    runtime_error("Invalid global name");
                    RETURN_RUNTIME_ERROR;
                }
                ObjString* name = constants[bx].as_string();
                auto it = globals_.find(name);
                if (it == globals_.end()) {
                    runtime_error("Undefined variable '%s'", name->value.c_str());
                    RETURN_RUNTIME_ERROR;
                }
                S(wa) = it->second;
                break;
            }
            case Opcode::SET_GLOBAL: {
                uint16_t bx = (wb << 8) | wc;
                auto& constants = frame->closure->function->constants;
                if (bx >= constants.size() || !constants[bx].is_string()) {
                    runtime_error("Invalid global name");
                    RETURN_RUNTIME_ERROR;
                }
                ObjString* name = constants[bx].as_string();
                globals_[name] = S(wa);
                break;
            }
            case Opcode::SET_LOCAL: stack_[frame->base_register + wb] = S(wa); break;
            case Opcode::GET_UPVALUE: {
                if (wb >= frame->closure->upvalues.size()) { runtime_error("Invalid upvalue index"); RETURN_RUNTIME_ERROR; }
                S(wa) = *frame->closure->upvalues[wb]->location;
                break;
            }
            case Opcode::SET_UPVALUE: {
                if (wb >= frame->closure->upvalues.size()) { runtime_error("Invalid upvalue index"); RETURN_RUNTIME_ERROR; }
                *frame->closure->upvalues[wb]->location = S(wa);
                break;
            }
            case Opcode::ADD: {
                Value& rb = S(wb); Value& rc = S(wc);
                if (rb.is_number() && rc.is_number()) {
                    S(wa) = Value(rb.get_number() + rc.get_number());
                } else if (rb.is_string() && rc.is_string()) {
                    auto* result = get_string_table().intern(rb.as_string()->value + rc.as_string()->value);
                    S(wa) = Value(static_cast<Obj*>(result));
                } else {
                    runtime_error("Operands must be two numbers or two strings");
                    RETURN_RUNTIME_ERROR;
                }
                break;
            }
            case Opcode::SUB: {
                Value& rb = S(wb); Value& rc = S(wc);
                if (rb.is_number() && rc.is_number()) { S(wa) = Value(rb.get_number() - rc.get_number()); }
                else { runtime_error("Operands must be two numbers"); RETURN_RUNTIME_ERROR; }
                break;
            }
            case Opcode::MUL: {
                Value& rb = S(wb); Value& rc = S(wc);
                if (rb.is_number() && rc.is_number()) { S(wa) = Value(rb.get_number() * rc.get_number()); }
                else { runtime_error("Operands must be two numbers"); RETURN_RUNTIME_ERROR; }
                break;
            }
            case Opcode::DIV: {
                Value& rb = S(wb); Value& rc = S(wc);
                if (rb.is_number() && rc.is_number()) {
                    if (rc.get_number() == 0) { runtime_error("Division by zero"); RETURN_RUNTIME_ERROR; }
                    S(wa) = Value(rb.get_number() / rc.get_number());
                } else { runtime_error("Operands must be two numbers"); RETURN_RUNTIME_ERROR; }
                break;
            }
            case Opcode::MOD: {
                Value& rb = S(wb); Value& rc = S(wc);
                if (rb.is_number() && rc.is_number()) {
                    if (rc.get_number() == 0) { runtime_error("Modulo by zero"); RETURN_RUNTIME_ERROR; }
                    S(wa) = Value(fmod(rb.get_number(), rc.get_number()));
                } else { runtime_error("Operands must be two numbers"); RETURN_RUNTIME_ERROR; }
                break;
            }
            case Opcode::NEG: {
                Value& rb = S(wb);
                if (rb.is_number()) { S(wa) = Value(-rb.get_number()); }
                else { runtime_error("Operand must be a number"); RETURN_RUNTIME_ERROR; }
                break;
            }
            case Opcode::EQ: S(wa) = Value(S(wb) == S(wc)); break;
            case Opcode::NEQ: S(wa) = Value(S(wb) != S(wc)); break;
            case Opcode::LT: {
                Value& rb = S(wb); Value& rc = S(wc);
                if (rb.is_number() && rc.is_number()) { S(wa) = Value(rb.get_number() < rc.get_number()); }
                else if (rb.is_string() && rc.is_string()) { S(wa) = Value(rb.as_string()->value < rc.as_string()->value); }
                else { runtime_error("Operands must be two numbers or two strings"); RETURN_RUNTIME_ERROR; }
                break;
            }
            case Opcode::LTE: {
                Value& rb = S(wb); Value& rc = S(wc);
                if (rb.is_number() && rc.is_number()) { S(wa) = Value(rb.get_number() <= rc.get_number()); }
                else if (rb.is_string() && rc.is_string()) { S(wa) = Value(rb.as_string()->value <= rc.as_string()->value); }
                else { runtime_error("Operands must be two numbers or two strings"); RETURN_RUNTIME_ERROR; }
                break;
            }
            case Opcode::GT: {
                Value& rb = S(wb); Value& rc = S(wc);
                if (rb.is_number() && rc.is_number()) { S(wa) = Value(rb.get_number() > rc.get_number()); }
                else if (rb.is_string() && rc.is_string()) { S(wa) = Value(rb.as_string()->value > rc.as_string()->value); }
                else { runtime_error("Operands must be two numbers or two strings"); RETURN_RUNTIME_ERROR; }
                break;
            }
            case Opcode::GTE: {
                Value& rb = S(wb); Value& rc = S(wc);
                if (rb.is_number() && rc.is_number()) { S(wa) = Value(rb.get_number() >= rc.get_number()); }
                else if (rb.is_string() && rc.is_string()) { S(wa) = Value(rb.as_string()->value >= rc.as_string()->value); }
                else { runtime_error("Operands must be two numbers or two strings"); RETURN_RUNTIME_ERROR; }
                break;
            }
            case Opcode::NOT: S(wa) = Value(!S(wb).is_truthy()); break;
            case Opcode::CLOSE_UPVALUE: {
                Value* slot_ptr = &stack_[frame->base_register + wa];
                while (open_upvalues_ && open_upvalues_->location >= slot_ptr) {
                    ObjUpvalue* uv = open_upvalues_;
                    uv->closed = *uv->location;
                    uv->location = &uv->closed;
                    open_upvalues_ = uv->next_upvalue;
                }
                break;
            }
            case Opcode::CALL: {
                // Redirect to shared CALL handler
                call_a = wa;
                call_b = wb;
                goto do_call;
            }
            case Opcode::RETURN: {
                Value result = S(wa);
                int callee_pos = frame->callee_stack_pos;
                int saved_top = frame->caller_stack_top;
                frame_count_--;
                if (frame_count_ == 0) {
                    stack_top_ = 0;
                    stack_[stack_top_++] = result;
                    return InterpretResult::Ok;
                }
                if (active_fiber_ && frame_count_ == active_fiber_->frame_base) {
                    active_fiber_->state = ObjFiber::State::Done;
                    active_fiber_ = active_fiber_->parent;
                }
                stack_top_ = std::max(saved_top, callee_pos + 1);
                stack_[callee_pos] = result;
                REFRESH_FRAME();
                break;
            }
            case Opcode::JMP: {
                int16_t offset = static_cast<int16_t>((wb << 8) | wc);
                // patch_jump subtracts 4 for WIDE, but JMP callers don't subtract INST_SIZE
                // so we need to add INST_SIZE back to compensate
                ip = wip + INST_SIZE + offset;
                break;
            }
            case Opcode::JMP_IF_FALSE: {
                int16_t offset = static_cast<int16_t>((wb << 8) | wc);
                if (!S(wa).is_truthy()) {
                    ip += offset;
                }
                break;
            }
            case Opcode::JMP_IF_TRUE: {
                int16_t offset = static_cast<int16_t>((wb << 8) | wc);
                if (S(wa).is_truthy()) {
                    ip += offset;
                }
                break;
            }
            case Opcode::NEW_ARRAY: {
                auto* arr = allocate_array();
                S(wa) = Value(static_cast<Obj*>(arr));
                break;
            }
            case Opcode::NEW_MAP: {
                auto* map = allocate_map();
                S(wa) = Value(static_cast<Obj*>(map));
                break;
            }
            case Opcode::GET_INDEX: {
                Value obj = S(wb);
                Value idx = S(wc);
                if (obj.is_array()) {
                    if (!idx.is_number()) { runtime_error("Array index must be a number"); RETURN_RUNTIME_ERROR; }
                    auto& elems = obj.as_array()->elements;
                    int i = static_cast<int>(idx.get_number());
                    if (i < 0) i += (int)elems.size();
                    if (i < 0 || i >= (int)elems.size()) { runtime_error("Array index out of bounds"); RETURN_RUNTIME_ERROR; }
                    S(wa) = elems[i];
                } else if (obj.is_map()) {
                    if (!idx.is_string()) { runtime_error("Map key must be a string"); RETURN_RUNTIME_ERROR; }
                    auto& entries = obj.as_map()->entries;
                    auto it = entries.find(idx.as_string()->value);
                    S(wa) = (it == entries.end()) ? Value() : it->second;
                } else if (obj.is_string()) {
                    if (!idx.is_number()) { runtime_error("String index must be a number"); RETURN_RUNTIME_ERROR; }
                    auto& s = obj.as_string()->value;
                    int i = static_cast<int>(idx.get_number());
                    if (i < 0) i += (int)s.size();
                    if (i < 0 || i >= (int)s.size()) { runtime_error("String index out of bounds"); RETURN_RUNTIME_ERROR; }
                    auto* ch = get_string_table().intern(std::string(1, s[i]));
                    S(wa) = Value(static_cast<Obj*>(ch));
                } else {
                    runtime_error("Cannot index this value");
                    RETURN_RUNTIME_ERROR;
                }
                break;
            }
            case Opcode::SET_INDEX: {
                Value obj = S(wa);
                Value idx = S(wb);
                Value val = S(wc);
                if (obj.is_array()) {
                    if (!idx.is_number()) { runtime_error("Array index must be a number"); RETURN_RUNTIME_ERROR; }
                    auto& elems = obj.as_array()->elements;
                    int i = static_cast<int>(idx.get_number());
                    if (i < 0) i += (int)elems.size();
                    if (i < 0) { runtime_error("Array index out of bounds"); RETURN_RUNTIME_ERROR; }
                    if (i >= (int)elems.size()) elems.resize(i + 1);
                    elems[i] = val;
                } else if (obj.is_map()) {
                    if (!idx.is_string()) { runtime_error("Map key must be a string"); RETURN_RUNTIME_ERROR; }
                    obj.as_map()->entries[idx.as_string()->value] = val;
                } else {
                    runtime_error("Cannot index this value");
                    RETURN_RUNTIME_ERROR;
                }
                break;
            }
            case Opcode::GET_FIELD: {
                Value obj = S(wb);
                auto& constants = frame->closure->function->constants;
                if (wc >= constants.size() || !constants[wc].is_string()) {
                    runtime_error("Invalid field name");
                    RETURN_RUNTIME_ERROR;
                }
                const std::string& field = constants[wc].as_string()->value;
                if (obj.is_instance()) {
                    auto& fields = obj.as_instance()->fields;
                    auto it = fields.find(field);
                    if (it != fields.end()) {
                        S(wa) = it->second;
                    } else {
                        auto& methods = obj.as_instance()->klass->methods;
                        auto meth_it = methods.find(field);
                        S(wa) = (meth_it != methods.end()) ? meth_it->second : Value();
                    }
                } else if (obj.is_map()) {
                    auto it = obj.as_map()->entries.find(field);
                    S(wa) = (it != obj.as_map()->entries.end()) ? it->second : Value();
                } else if (obj.is_class()) {
                    auto& methods = obj.as_class()->methods;
                    auto it = methods.find(field);
                    S(wa) = (it != methods.end()) ? it->second : Value();
                } else if (obj.is_string()) {
                    if (field == "length" || field == HASH_LENGTH) {
                        S(wa) = Value(static_cast<double>(obj.as_string()->value.size()));
                    } else {
                        S(wa) = Value();
                    }
                } else if (obj.is_array()) {
                    if (field == "length" || field == HASH_LENGTH) {
                        S(wa) = Value(static_cast<double>(obj.as_array()->elements.size()));
                    } else {
                        S(wa) = Value();
                    }
                } else {
                    S(wa) = Value();
                }
                break;
            }
            case Opcode::SET_FIELD: {
                Value obj = S(wa);
                auto& constants = frame->closure->function->constants;
                if (wb >= constants.size() || !constants[wb].is_string()) {
                    runtime_error("Invalid field name");
                    RETURN_RUNTIME_ERROR;
                }
                std::string field = constants[wb].as_string()->value;
                if (obj.is_instance()) {
                    obj.as_instance()->fields[field] = S(wc);
                } else if (obj.is_class()) {
                    obj.as_class()->methods[field] = S(wc);
                } else if (obj.is_map()) {
                    obj.as_map()->entries[field] = S(wc);
                } else {
                    runtime_error("Cannot set field on this value");
                    RETURN_RUNTIME_ERROR;
                }
                break;
            }
            case Opcode::NEW_CLASS: {
                uint16_t bx = (wb << 8) | wc;
                auto& constants = frame->closure->function->constants;
                if (bx >= constants.size() || !constants[bx].is_string()) {
                    runtime_error("Invalid class name");
                    RETURN_RUNTIME_ERROR;
                }
                auto* cls = allocate_class(constants[bx].as_string()->value);
                S(wa) = Value(static_cast<Obj*>(cls));
                break;
            }
            case Opcode::NEW_INSTANCE: {
                if (!S(wb).is_class()) {
                    runtime_error("Cannot instantiate non-class");
                    RETURN_RUNTIME_ERROR;
                }
                auto* inst = allocate_instance(S(wb).as_class());
                S(wa) = Value(static_cast<Obj*>(inst));
                break;
            }
            case Opcode::GET_METHOD: {
                auto& constants = frame->closure->function->constants;
                if (wc >= constants.size() || !constants[wc].is_string()) {
                    S(wa) = Value();
                    break;
                }
                std::string method = constants[wc].as_string()->value;
                Value obj = S(wb);
                if (obj.is_instance()) {
                    auto& methods = obj.as_instance()->klass->methods;
                    auto it = methods.find(method);
                    S(wa) = (it != methods.end()) ? it->second : Value();
                } else {
                    S(wa) = Value();
                }
                break;
            }
            case Opcode::NEW_RANGE: {
                if (!S(wb).is_number() || !S(wc).is_number()) {
                    runtime_error("Range bounds must be numbers");
                    RETURN_RUNTIME_ERROR;
                }
                auto* map = allocate_map();
                map->entries["start"] = S(wb);
                map->entries["end"] = S(wc);
                map->entries["__type__"] = Value(static_cast<Obj*>(get_string_table().intern("range")));
                S(wa) = Value(static_cast<Obj*>(map));
                break;
            }
            case Opcode::ITER_INIT: {
                Value iterable = S(wb);
                auto* iter = allocate_iterator();
                if (iterable.is_array()) {
                    iter->kind = ObjIterator::ArrayIter;
                    iter->arr = iterable.as_array();
                    iter->arr_index = 0;
                } else if (iterable.is_map() && iterable.as_map()->entries.count("__type__") &&
                           iterable.as_map()->entries["__type__"].is_string() &&
                           iterable.as_map()->entries["__type__"].as_string()->value == "range") {
                    double start_val = iterable.as_map()->entries["start"].get_number();
                    double end_val = iterable.as_map()->entries["end"].get_number();
                    iter->kind = ObjIterator::RangeIter;
                    iter->range_current = start_val;
                    iter->range_end = end_val;
                    iter->range_step = start_val <= end_val ? 1.0 : -1.0;
                } else if (iterable.is_string()) {
                    iter->kind = ObjIterator::StringIter;
                    iter->str = iterable.as_string();
                    iter->str_index = 0;
                } else {
                    runtime_error("Cannot iterate this value");
                    RETURN_RUNTIME_ERROR;
                }
                S(wa) = Value(static_cast<Obj*>(iter));
                break;
            }
            case Opcode::ITER_NEXT: {
                Value iter_val = S(wb);
                if (!iter_val.is_iterator()) {
                    runtime_error("Invalid iterator");
                    RETURN_RUNTIME_ERROR;
                }
                auto* iter = iter_val.as_iterator();
                if (iter->kind == ObjIterator::ArrayIter) {
                    if (iter->arr_index < (int)iter->arr->elements.size()) {
                        S(wa) = iter->arr->elements[iter->arr_index++];
                    } else {
                        iter->done = true;
                        S(wa) = Value();
                    }
                } else if (iter->kind == ObjIterator::RangeIter) {
                    bool done = (iter->range_step > 0) ? (iter->range_current > iter->range_end) : (iter->range_current < iter->range_end);
                    if (!done) {
                        S(wa) = Value(iter->range_current);
                        iter->range_current += iter->range_step;
                    } else {
                        iter->done = true;
                        S(wa) = Value();
                    }
                } else { // StringIter
                    auto& s = iter->str->value;
                    if (iter->str_index < (int)s.size()) {
                        auto* ch = get_string_table().intern(std::string(1, s[iter->str_index++]));
                        S(wa) = Value(static_cast<Obj*>(ch));
                    } else {
                        iter->done = true;
                        S(wa) = Value();
                    }
                }
                break;
            }
            case Opcode::ITER_DONE: {
                Value iter = S(wb);
                if (iter.is_iterator()) {
                    S(wa) = Value(iter.as_iterator()->done);
                } else {
                    S(wa) = Value(true);
                }
                break;
            }
            case Opcode::PRINT: {
                std::cout << S(wa).to_string() << std::endl;
                break;
            }
            case Opcode::FIBER_YIELD: {
                if (skip_native_call_) {
                    skip_native_call_ = false;
                    S(wa) = skip_native_result_;
                    break;
                }
                yield_pending_ = true;
                yield_value_ = S(wa);
                break;
            }
            case Opcode::FIBER_RESUME: {
                // Save caller's IP before switching to fiber frame
                frame->ip = ip;
                Value fiber_val = S(wb);
                Value resume_val = S(wc);
                if (!fiber_val.is_fiber()) {
                    runtime_error("Can only resume a fiber");
                    RETURN_RUNTIME_ERROR;
                }
                auto* fiber = fiber_val.as_fiber();
                if (fiber->state == ObjFiber::State::Done) {
                    runtime_error("Cannot resume a finished fiber");
                    RETURN_RUNTIME_ERROR;
                }
                if (fiber->state == ObjFiber::State::Suspended) {
                    int caller_base = frame->base_register;
                    stack_top_ = caller_base;
                    if (stack_top_ + (int)fiber->saved_stack.size() > MAX_STACK) {
                        runtime_error("Stack overflow resuming fiber");
                        RETURN_RUNTIME_ERROR;
                    }
                    for (size_t i = 0; i < fiber->saved_stack.size(); i++) {
                        stack_[stack_top_++] = fiber->saved_stack[i];
                    }
                    int new_frame_base = frame_count_;
                    if (new_frame_base + fiber->saved_frame_count > MAX_FRAMES) {
                        runtime_error("Frame overflow resuming fiber");
                        RETURN_RUNTIME_ERROR;
                    }
                    frame_count_ = new_frame_base + fiber->saved_frame_count;
                    for (int i = 0; i < fiber->saved_frame_count; i++) {
                        auto& sf = fiber->saved_frames[i];
                        auto& f = frames_[new_frame_base + i];
                        f.base_register = sf.base_register + caller_base;
                        f.return_register = sf.return_register;
                        f.callee_stack_pos = sf.callee_stack_pos + caller_base;
                        f.caller_stack_top = sf.caller_stack_top;
                        // sf.callee_stack_pos is already an offset from save_start (i.e., into saved_stack)
                        if (sf.callee_stack_pos >= 0 && sf.callee_stack_pos < (int)fiber->saved_stack.size() && fiber->saved_stack[sf.callee_stack_pos].is_closure()) {
                            f.closure = fiber->saved_stack[sf.callee_stack_pos].as_closure();
                            f.ip = f.closure->function->bytecode.data() + sf.ip_offset;
                        }
                    }
                    fiber->frame_base = new_frame_base;
                    open_upvalues_ = fiber->saved_open_upvalues;
                    fiber->state = ObjFiber::State::Running;
                    fiber->parent = active_fiber_;
                    fiber->resume_return_reg = wa;
                    active_fiber_ = fiber;
                    skip_native_call_ = true;
                    skip_native_result_ = resume_val;
                    REFRESH_FRAME();
                    break;
                }
                // Created state: start the fiber
                fiber->state = ObjFiber::State::Running;
                fiber->parent = active_fiber_;
                fiber->resume_return_reg = wa;
                active_fiber_ = fiber;
                int callee_abs = stack_top_;
                stack_[stack_top_++] = Value(static_cast<Obj*>(fiber->entry));
                int arg_count = 0;
                if (!resume_val.is_nil()) {
                    stack_[stack_top_++] = resume_val;
                    arg_count = 1;
                } else {
                    arg_count = (int)fiber->initial_args.size();
                    for (int i = 0; i < arg_count; i++) {
                        stack_[stack_top_++] = fiber->initial_args[i];
                    }
                }
                fiber->frame_base = frame_count_;
                frame->ip = ip; if (!call(fiber->entry, arg_count, wa, callee_abs)) {
                    active_fiber_ = nullptr;
                    RETURN_RUNTIME_ERROR;
                }
                REFRESH_FRAME();
                break;
            }
            case Opcode::TAIL_CALL: {
                int arg_count = wb;
                int callee_abs = frame->base_register + wa;
                Value callee = stack_[callee_abs];
                if (callee.is_closure()) {
                    auto* closure = callee.as_closure();
                    if (arg_count != closure->function->arity) {
                        runtime_error("Expected %d arguments but got %d", closure->function->arity, arg_count);
                        RETURN_RUNTIME_ERROR;
                    }
                    while (open_upvalues_ && open_upvalues_->location >= &stack_[frame->base_register]) {
                        ObjUpvalue* uv = open_upvalues_;
                        uv->closed = *uv->location;
                        uv->location = &uv->closed;
                        open_upvalues_ = uv->next_upvalue;
                    }
                    int args_src = callee_abs + 1;
                    for (int i = 0; i < arg_count; i++) {
                        stack_[frame->base_register + i] = stack_[args_src + i];
                    }
                    int needed = closure->function->register_count;
                    for (int i = arg_count; i < needed; i++) {
                        stack_[frame->base_register + i] = Value();
                    }
                    frame->closure = closure;
                    ip = closure->function->bytecode.data();
                    stack_top_ = frame->base_register + needed;
                } else if (callee.is_native()) {
                    auto* native = callee.as_native();
                    Value* args = &stack_[callee_abs + 1];
                    Value result = native->function(arg_count, args);
                    int callee_pos = frame->callee_stack_pos;
                    frame_count_--;
                    if (frame_count_ == 0) {
                        stack_top_ = 0;
                        stack_[stack_top_++] = result;
                        return InterpretResult::Ok;
                    }
                    stack_top_ = callee_pos;
                    stack_[stack_top_++] = result;
                    REFRESH_FRAME();
                } else if (callee.is_class()) {
                    auto* instance = allocate_instance(callee.as_class());
                    Value instance_val = Value(static_cast<Obj*>(instance));
                    stack_[callee_abs] = instance_val;
                    auto& methods = callee.as_class()->methods;
                    auto init_it = methods.find("init");
                    if (init_it == methods.end()) init_it = methods.find(akar_hash_symbol("init"));
                    if (init_it != methods.end() && init_it->second.is_closure()) {
                        for (int i = arg_count; i > 0; i--) {
                            stack_[callee_abs + 1 + i] = stack_[callee_abs + 1 + i - 1];
                        }
                        stack_[callee_abs + 1] = instance_val;
                        stack_top_ += 1;
                        auto* init_closure = init_it->second.as_closure();
                        while (open_upvalues_ && open_upvalues_->location >= &stack_[frame->base_register]) {
                            ObjUpvalue* uv = open_upvalues_;
                            uv->closed = *uv->location;
                            uv->location = &uv->closed;
                            open_upvalues_ = uv->next_upvalue;
                        }
                        int args_src = callee_abs + 1;
                        int total_args = arg_count + 1;
                        for (int i = 0; i < total_args; i++) {
                            stack_[frame->base_register + i] = stack_[args_src + i];
                        }
                        int needed = init_closure->function->register_count;
                        for (int i = total_args; i < needed; i++) {
                            stack_[frame->base_register + i] = Value();
                        }
                        frame->closure = init_closure;
                        ip = init_closure->function->bytecode.data();
                        stack_top_ = frame->base_register + needed;
                    }
                } else {
                    runtime_error("Tail call target must be a function or class");
                    RETURN_RUNTIME_ERROR;
                }
                break;
            }
            case Opcode::AWAIT: {
                Value val = S(wa);
                if (val.is_nil()) {
                    int callee_pos = frame->callee_stack_pos;
                    int saved_top = frame->caller_stack_top;
                    frame_count_--;
                    if (frame_count_ == 0) {
                        stack_top_ = 0;
                        stack_[stack_top_++] = Value();
                        return InterpretResult::Ok;
                    }
                    stack_top_ = std::max(saved_top, callee_pos + 1);
                    stack_[callee_pos] = Value();
                    REFRESH_FRAME();
                    break;
                }
                break;
            }
            case Opcode::THROW: {
                Value exception = S(wa);
                std::string msg = exception.to_string();
                if (try_count_ > 0) {
                    TryFrame& tf = try_frames_[--try_count_];
                    while (frame_count_ > tf.frame_count) {
                        frame_count_--;
                    }
                    REFRESH_FRAME();
                    ip = tf.catch_ip;
                    if (exception.is_string()) {
                        S(tf.catch_register) = exception;
                    } else {
                        auto* err_str = get_string_table().intern(msg);
                        S(tf.catch_register) = Value(static_cast<Obj*>(err_str));
                    }
                    DISPATCH();
                }
                runtime_error("%s", msg.c_str());
                RETURN_RUNTIME_ERROR;
            }
            case Opcode::TRY_BEGIN: {
                uint16_t bx = (wb << 8) | wc;
                if (try_count_ >= MAX_TRY) {
                    runtime_error("Too many nested try blocks");
                    RETURN_RUNTIME_ERROR;
                }
                TryFrame& tf = try_frames_[try_count_++];
                tf.frame_count = frame_count_;
                tf.catch_ip = ip + static_cast<int16_t>(bx);
                tf.catch_register = wa;
                break;
            }
            case Opcode::TRY_END: {
                uint16_t bx = (wb << 8) | wc;
                if (try_count_ > 0) try_count_--;
                if (bx > 0) {
                    ip += bx;
                }
                break;
            }
            case Opcode::CLOSURE: {
                uint16_t bx = (wb << 8) | wc;
                auto& constants = frame->closure->function->constants;
                if (bx >= constants.size() || !constants[bx].is_obj() ||
                    constants[bx].as_obj()->type != ObjType::Function) {
                    runtime_error("Invalid closure constant");
                    RETURN_RUNTIME_ERROR;
                }
                auto* func = static_cast<ObjFunction*>(constants[bx].as_obj());
                auto* closure = allocate_closure(func);
                for (auto& desc : func->upvalue_descs) {
                    ObjUpvalue* upvalue = nullptr;
                    if (desc.is_local) {
                        Value* local_slot = &stack_[frame->base_register + desc.index];
                        ObjUpvalue** pp = &open_upvalues_;
                        while (*pp && (*pp)->location > local_slot) {
                            pp = &(*pp)->next_upvalue;
                        }
                        if (*pp && (*pp)->location == local_slot) {
                            upvalue = *pp;
                        } else {
                            upvalue = allocate_upvalue(local_slot);
                            upvalue->next_upvalue = *pp;
                            *pp = upvalue;
                        }
                    } else {
                        upvalue = frame->closure->upvalues[desc.index];
                    }
                    closure->upvalues.push_back(upvalue);
                }
                S(wa) = Value(static_cast<Obj*>(closure));
                break;
            }
            case Opcode::HALT:
                return InterpretResult::Ok;
            case Opcode::NOP:
                break;
            default:
                runtime_error("Opcode %d cannot be used with WIDE prefix", wide_op);
                RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }

    // === Signal & Effect opcodes ===
    CASE(SIGNAL_CREATE): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        auto* sig = allocate_signal(S(b), "signal");
        S(a) = Value(static_cast<Obj*>(sig));
        DISPATCH();
    }
    CASE(SIGNAL_GET): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        Value sig_val = S(b);
        if (!sig_val.is_signal()) {
            runtime_error("SIGNAL_GET: not a signal");
            RETURN_RUNTIME_ERROR;
        }
        auto* sig = sig_val.as_signal();
        S(a) = sig->value;
        // Track dependency if inside an effect
        if (current_effect_) {
            // Check if already subscribed
            bool found = false;
            for (auto* dep : current_effect_->dependencies) {
                if (dep == sig) { found = true; break; }
            }
            if (!found) {
                current_effect_->dependencies.push_back(sig);
                sig->subscribers.push_back(current_effect_);
                // Profiler: record signal read (only when tracking dependency)
                if (__builtin_expect(profiler_.is_profiling(), 0)) {
                    profiler_.record_signal_read(sig->name.c_str());
                }
            }
        }
        DISPATCH();
    }
    CASE(SIGNAL_SET): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        Value sig_val = S(a);
        if (!sig_val.is_signal()) {
            runtime_error("SIGNAL_SET: not a signal");
            RETURN_RUNTIME_ERROR;
        }
        auto* sig = sig_val.as_signal();
        sig->value = S(b);
        // Profiler: record signal write
        if (__builtin_expect(profiler_.is_profiling(), 0)) {
            profiler_.record_signal_write(sig->name.c_str());
        }
        // Queue dependent effects for re-execution
        for (auto* eff : sig->subscribers) {
            if (eff->state != ObjEffect::State::Queued && eff->body) {
                eff->state = ObjEffect::State::Queued;
                effect_queue_.push_back(eff);
            }
        }
        DISPATCH();
    }
    CASE(EFFECT_CREATE): {
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        ip += 4;
        Value closure_val = S(b);
        if (!closure_val.is_closure()) {
            runtime_error("EFFECT_CREATE: body must be a closure");
            RETURN_RUNTIME_ERROR;
        }
        auto* eff = allocate_effect(closure_val.as_closure(), "effect");
        S(a) = Value(static_cast<Obj*>(eff));
        DISPATCH();
    }
    CASE(EFFECT_RUN): {
        uint8_t a = ip[1];
        ip += 4;
        Value eff_val = S(a);
        if (!eff_val.is_effect()) {
            runtime_error("EFFECT_RUN: not an effect");
            RETURN_RUNTIME_ERROR;
        }
        auto* eff = eff_val.as_effect();
        if (!eff->body) DISPATCH();

        // Profiler: record effect run
        if (__builtin_expect(profiler_.is_profiling(), 0)) {
            profiler_.record_effect_run(eff->name.c_str(), false);
        }

        // Clear old dependencies
        for (auto* old_sig : eff->dependencies) {
            auto& subs = old_sig->subscribers;
            subs.erase(std::remove(subs.begin(), subs.end(), eff), subs.end());
        }
        eff->dependencies.clear();
        eff->state = ObjEffect::State::Running;

        // Set current_effect_ BEFORE calling so SIGNAL_GET tracks dependencies
        current_effect_ = eff;
        effect_frame_depth_ = frame_count_ + 1; // effect will be at this depth

        // Push the effect body closure and call it directly.
        // The effect body will execute inline in the current run() loop.
        VLOG("[EFFECT_RUN] calling effect body, frame_count=%d, stack_top=%d\n", frame_count_, stack_top_);
        frame->ip = ip;
        int save_top = stack_top_;
        stack_[stack_top_++] = Value(static_cast<Obj*>(eff->body));
        int callee_pos = save_top;
        if (!call(eff->body, 0, callee_pos, callee_pos)) {
            current_effect_ = nullptr;
            RETURN_RUNTIME_ERROR;
        }
        // Refresh frame/base/ip to point to the new effect body frame
        REFRESH_FRAME();
        // The effect body will now execute via DISPATCH (it's the new top frame).
        // When it RETURNs, the RETURN handler will pop the frame and
        // DISPATCH to the next instruction after EFFECT_RUN.
        DISPATCH();
    }

    // === Enum opcodes ===
    CASE(ENUM_CREATE): {
        uint8_t a = ip[1];
        uint16_t bx = (ip[2] << 8) | ip[3];
        ip += 4;
        auto& constants = frame->closure->function->constants;
        if (bx >= constants.size() || !constants[bx].is_string()) {
            runtime_error("Invalid enum name constant");
            RETURN_RUNTIME_ERROR;
        }
        ObjString* name = constants[bx].as_string();
        auto* cls = allocate_class(name->value);
        // Store type_id as a special field
        uint16_t tid = enum_type_counter_++;
        cls->methods["_type_id"] = Value(static_cast<double>(tid));
        S(a) = Value(static_cast<Obj*>(cls));
        DISPATCH();
    }
    CASE(ENUM_VARIANT): {
        uint8_t a = ip[1];  // class register
        uint8_t b = ip[2];  // variant name constant index
        uint8_t c = ip[3];  // constant index containing variant index
        ip += 4;
        auto& constants = frame->closure->function->constants;
        if (!S(a).is_class()) {
            runtime_error("ENUM_VARIANT: not a class");
            RETURN_RUNTIME_ERROR;
        }
        auto* cls = S(a).as_class();
        if (b >= constants.size() || !constants[b].is_string()) {
            runtime_error("Invalid variant name constant");
            RETURN_RUNTIME_ERROR;
        }
        ObjString* variant_name = constants[b].as_string();
        // Get type_id from class
        double type_id_d = 0;
        auto tid_it = cls->methods.find("_type_id");
        if (tid_it != cls->methods.end() && tid_it->second.is_number()) {
            type_id_d = tid_it->second.get_number();
        }
        uint16_t type_id = static_cast<uint16_t>(type_id_d);
        // Read variant index from constant pool
        uint16_t variant_idx = 0;
        if (c < constants.size() && constants[c].is_number()) {
            variant_idx = static_cast<uint16_t>(constants[c].get_number());
        }
        // Create NaN-boxed enum value
        Value enum_val = make_enum_value(type_id, variant_idx);
        cls->methods[variant_name->value] = enum_val;
        DISPATCH();
    }
    CASE(ENUM_DATA_VARIANT): {
        uint8_t a = ip[1];  // class register
        uint8_t b = ip[2];  // variant name constant index
        ip += 4;
        auto& constants = frame->closure->function->constants;
        if (!S(a).is_class()) {
            runtime_error("ENUM_DATA_VARIANT: not a class");
            RETURN_RUNTIME_ERROR;
        }
        auto* cls = S(a).as_class();
        if (b >= constants.size() || !constants[b].is_string()) {
            runtime_error("Invalid variant name constant");
            RETURN_RUNTIME_ERROR;
        }
        ObjString* variant_name = constants[b].as_string();
        // For data variants, we store a special marker.
        // When called as Enum.Variant(value), the VM creates an instance with the value.
        // We'll store a string marker that the GET_FIELD handler recognizes.
        auto* marker = get_string_table().intern("__data_variant__:" + variant_name->value);
        cls->methods[variant_name->value] = Value(static_cast<Obj*>(marker));
        DISPATCH();
    }
    CASE(ENUM_GET): {
        // A = R[B].variant_C — get enum variant from class
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        auto& constants = frame->closure->function->constants;
        Value obj = S(b);
        if (obj.is_class() && c < constants.size() && constants[c].is_string()) {
            ObjString* name = constants[c].as_string();
            auto it = obj.as_class()->methods.find(name->value);
            S(a) = (it != obj.as_class()->methods.end()) ? it->second : Value();
        } else {
            S(a) = Value();
        }
        DISPATCH();
    }
    CASE(ENUM_IS): {
        // A = is_enum_type(R[B], name_const_C) — check if value belongs to enum
        uint8_t a = ip[1];
        uint8_t b = ip[2];
        uint8_t c = ip[3];
        ip += 4;
        auto& constants = frame->closure->function->constants;
        Value val = S(b);
        if (c >= constants.size() || !constants[c].is_string()) {
            S(a) = Value(false);
            DISPATCH();
        }
        ObjString* enum_name = constants[c].as_string();
        // Simple enum values: NaN-boxed, check by looking up the enum class's _type_id
        if (is_enum_value(val)) {
            // Find the enum class by name in globals
            auto it = globals_.find(enum_name);
            if (it != globals_.end() && it->second.is_class()) {
                auto tid_it = it->second.as_class()->methods.find("_type_id");
                if (tid_it != it->second.as_class()->methods.end() && tid_it->second.is_number()) {
                    uint16_t expected_tid = static_cast<uint16_t>(tid_it->second.get_number());
                    S(a) = Value(enum_type_id(val) == expected_tid);
                } else {
                    S(a) = Value(false);
                }
            } else {
                S(a) = Value(false);
            }
        } else if (val.is_instance()) {
            // Data variant instances have _enum_type field
            auto ft = val.as_instance()->fields.find("_enum_type");
            if (ft != val.as_instance()->fields.end() && ft->second.is_string()) {
                S(a) = Value(ft->second.as_string()->value == enum_name->value);
            } else {
                S(a) = Value(false);
            }
        } else {
            S(a) = Value(false);
        }
        DISPATCH();
    }

    } // end for(;;)

#else
    // Fallback: switch-based dispatch for non-GCC compilers
    for (;;) {
    loop_continue:
        HANDLE_FIBER_YIELD();
        CHECK_MEMORY_LIMIT();
        uint8_t instruction = ip[0];
        Opcode op = static_cast<Opcode>(instruction);
        switch (op) {
            case Opcode::LOAD_CONST: {
                uint8_t a = ip[1]; uint16_t bx = (ip[2] << 8) | ip[3]; ip += 4;
                S(a) = frame->closure->function->constants[bx]; break;
            }
            case Opcode::LOAD_NIL: { uint8_t a = ip[1]; ip += 4; S(a) = Value(); break; }
            case Opcode::LOAD_TRUE: { uint8_t a = ip[1]; ip += 4; S(a) = Value(true); break; }
            case Opcode::LOAD_FALSE: { uint8_t a = ip[1]; ip += 4; S(a) = Value(false); break; }
            case Opcode::MOVE: { uint8_t a = ip[1]; uint8_t b = ip[2]; ip += 4; S(a) = S(b); break; }
            case Opcode::HALT: return InterpretResult::Ok;
            case Opcode::NOP: ip += 4; break;
            default: runtime_error("Unknown opcode"); RETURN_RUNTIME_ERROR;
        }
    }
#endif
}

Value VM::call_function(ObjClosure* closure, const std::vector<Value>& args) {
    // Save state
    int saved_top = stack_top_;

    // Push closure and args
    int callee_abs = stack_top_;
    stack_[stack_top_++] = Value(static_cast<Obj*>(closure));
    for (auto& arg : args) {
        stack_[stack_top_++] = arg;
    }

    if (!call(closure, args.size(), 0, callee_abs)) {
        stack_top_ = saved_top;
        return Value();
    }

    auto result = run();
    if (result != InterpretResult::Ok) {
        stack_top_ = saved_top;
        return Value();
    }
    Value ret = stack_[stack_top_ - 1];
    stack_top_ = saved_top;
    return ret;
}

bool VM::call(ObjClosure* closure, int arg_count, int return_reg, int callee_abs) {
    if (frame_count_ >= MAX_FRAMES) {
        runtime_error("Stack overflow");
        return false;
    }

    int fixed_arity = closure->function->arity;
    bool has_varargs = closure->function->has_varargs;

    if (has_varargs) {
        if (arg_count < fixed_arity) {
            runtime_error("Expected at least %d arguments but got %d", fixed_arity, arg_count);
            return false;
        }
    } else {
        if (arg_count != fixed_arity) {
            runtime_error("Expected %d arguments but got %d", fixed_arity, arg_count);
            return false;
        }
    }

    int args_abs = callee_abs + 1;  // absolute position of first argument

    // For varargs: collect extra args into an array
    if (has_varargs) {
        auto* arr = allocate_array();
        if (arg_count > fixed_arity) {
            for (int i = 0; i < arg_count - fixed_arity; i++) {
                arr->elements.push_back(stack_[args_abs + fixed_arity + i]);
            }
        }
        stack_[args_abs + fixed_arity] = Value(static_cast<Obj*>(arr));
        stack_top_ = args_abs + fixed_arity + 1;
    }

    CallFrame* frame = &frames_[frame_count_++];
    frame->closure = closure;
    frame->ip = closure->function->bytecode.data();
    frame->base_register = args_abs;  // slot(0) = first argument
    frame->return_register = return_reg;
    frame->callee_stack_pos = callee_abs;
    frame->caller_stack_top = stack_top_;
    frame->slots = &stack_[args_abs];

    // Initialize remaining registers to nil (skip varargs register)
    int needed = closure->function->register_count;
    int varargs_slot = has_varargs ? fixed_arity : -1;
    for (int i = arg_count; i < needed; i++) {
        if (i == varargs_slot) continue;  // don't overwrite varargs array
        stack_[args_abs + i] = Value();
    }
    int new_top = args_abs + needed;
    if (new_top > MAX_STACK) { runtime_error("Stack overflow"); return false; }
    if (new_top > stack_top_) stack_top_ = new_top;

    return true;
}

bool VM::call_native(ObjNative* native, int arg_count, int /*return_reg*/, int callee_abs) {
    Value* args = &stack_[callee_abs + 1];
    Value result = native->function(arg_count, args);
    stack_top_ = callee_abs;
    stack_[stack_top_++] = result; // push result at callee position
    return true;
}

void VM::pop_frame() {
    frame_count_--;
}

Value& VM::slot(int index) {
    int absolute = frames_[frame_count_ - 1].base_register + index;
    return stack_[absolute];
}

void VM::push(Value value) {
    stack_[stack_top_++] = value;
}

Value VM::pop() {
    return stack_[--stack_top_];
}

bool VM::throw_to_catch(const std::string& msg) {
    if (try_count_ <= 0) return false;
    TryFrame& tf = try_frames_[--try_count_];
    while (frame_count_ > tf.frame_count) {
        // Close upvalues at this frame's base before unwinding
        Value* slot_ptr = &stack_[frames_[frame_count_ - 1].base_register];
        while (open_upvalues_ && open_upvalues_->location >= slot_ptr) {
            ObjUpvalue* uv = open_upvalues_;
            uv->closed = *uv->location;
            uv->location = &uv->closed;
            open_upvalues_ = uv->next_upvalue;
        }
        frame_count_--;
    }
    CallFrame* f = &frames_[frame_count_ - 1];
    f->ip = tf.catch_ip;
    auto* err_str = get_string_table().intern(msg);
    // Store exception directly in the stack at the correct position
    int abs_pos = f->base_register + tf.catch_register;
    stack_[abs_pos] = Value(static_cast<Obj*>(err_str));
    return true;
}

void VM::runtime_error(const char* format, ...) {
    char buf[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    last_error_ = buf;

    // Check if we can throw to a catch block
    if (throw_to_catch(last_error_)) {
        exception_caught_ = true;
        return;
    }

    // Print stack trace
    for (int i = frame_count_ - 1; i >= 0; i--) {
        auto* fr = &frames_[i];
        auto* func = fr->closure->function;
        size_t offset = fr->ip - func->bytecode.data();
        fprintf(stderr, "  [line %d] in %s (ip=%zu)\n", func->line, func->name.c_str(), offset);
    }
    fprintf(stderr, "Error: %s\n", buf);
}

void VM::define_native(const std::string& name, NativeFn function) {
    auto* native = allocate_native(function, name);
    ObjString* key = get_string_table().intern(name);
    globals_[key] = Value(static_cast<Obj*>(native));
    // Also register with hashed name so release .ako files can find it
    auto* native2 = allocate_native(std::move(function), name);
    ObjString* hash_key = get_string_table().intern(akar_hash_symbol(name));
    globals_[hash_key] = Value(static_cast<Obj*>(native2));
}

void VM::set_global(const std::string& name, Value value) {
    ObjString* key = get_string_table().intern(name);
    globals_[key] = value;
}

Value VM::get_global(const std::string& name) const {
    ObjString* key = get_string_table().intern(name);
    auto it = globals_.find(key);
    if (it != globals_.end()) return it->second;
    return Value();
}

} // namespace akar
