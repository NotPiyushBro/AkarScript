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
}

void VM::collect_garbage() {
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

    // Helper: refresh frame and base after call/return
    #define REFRESH_FRAME() do { frame = &frames_[frame_count_ - 1]; base = frame->base_register; } while(0)

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
    #define CHECK_MEMORY_LIMIT() do { \
        if (++opcode_counter >= 1024) { \
            opcode_counter = 0; \
            if (get_allocated_bytes() >= get_next_gc()) { \
                collect_garbage(); \
                if (memory_limit_exceeded()) { \
                    runtime_error("Memory limit exceeded (%zu bytes)", get_memory_limit()); \
                    RETURN_RUNTIME_ERROR; \
                } \
            } \
        } \
    } while(0)

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
        &&op_FIBER_YIELD, &&op_FIBER_RESUME, &&op_TAIL_CALL,
        &&op_AWAIT, &&op_THROW, &&op_TRY_BEGIN, &&op_TRY_END,
    };

    #define DISPATCH() do { CHECK_MEMORY_LIMIT(); goto *dispatch_table[frame->ip[0]]; } while(0)
    #define CASE(op) op_##op

    for (;;) {
    loop_continue:
        DISPATCH();

    CASE(LOAD_CONST): {
        uint8_t a = frame->ip[1];
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        frame->ip += 4;
        S(a) = frame->closure->function->constants[bx];
        DISPATCH();
    }
    CASE(LOAD_NIL): {
        uint8_t a = frame->ip[1];
        frame->ip += 4;
        S(a) = Value();
        DISPATCH();
    }
    CASE(LOAD_TRUE): {
        uint8_t a = frame->ip[1];
        frame->ip += 4;
        S(a) = Value(true);
        DISPATCH();
    }
    CASE(LOAD_FALSE): {
        uint8_t a = frame->ip[1];
        frame->ip += 4;
        S(a) = Value(false);
        DISPATCH();
    }
    CASE(MOVE): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        S(a) = S(b);
        DISPATCH();
    }
    CASE(GET_LOCAL): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        S(a) = stack_[base + b];
        DISPATCH();
    }
    CASE(SET_LOCAL): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        stack_[base + b] = S(a);
        DISPATCH();
    }
    CASE(GET_UPVALUE): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        S(a) = *frame->closure->upvalues[b]->location;
        DISPATCH();
    }
    CASE(SET_UPVALUE): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        *frame->closure->upvalues[b]->location = S(a);
        DISPATCH();
    }
    CASE(GET_GLOBAL): {
        uint8_t a = frame->ip[1];
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        frame->ip += 4;
        auto& constants = frame->closure->function->constants;
        if (bx >= constants.size() || !constants[bx].is_string()) {
            runtime_error("Invalid global name");
            RETURN_RUNTIME_ERROR;
        }
        ObjString* name = constants[bx].as_string();
        auto it = globals_.find(name->value);
        if (it == globals_.end()) {
            runtime_error("Undefined variable '%s'", name->value.c_str());
            RETURN_RUNTIME_ERROR;
        }
        S(a) = it->second;
        DISPATCH();
    }
    CASE(SET_GLOBAL): {
        uint8_t a = frame->ip[1];
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        frame->ip += 4;
        auto& constants = frame->closure->function->constants;
        if (bx >= constants.size() || !constants[bx].is_string()) {
            runtime_error("Invalid global name");
            RETURN_RUNTIME_ERROR;
        }
        ObjString* name = constants[bx].as_string();
        globals_[name->value] = S(a);
        DISPATCH();
    }
    CASE(ADD): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() + rc.get_number());
        } else if (rb.is_string() && rc.is_string()) {
            auto* result = get_string_table().intern(
                rb.as_string()->value + rc.as_string()->value);
            S(a) = Value(static_cast<Obj*>(result));
        } else {
            runtime_error("Operands must be two numbers or two strings");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(SUB): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (!rb.is_number() || !rc.is_number()) {
            runtime_error("Operands must be numbers");
            RETURN_RUNTIME_ERROR;
        }
        S(a) = Value(rb.get_number() - rc.get_number());
        DISPATCH();
    }
    CASE(MUL): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (!rb.is_number() || !rc.is_number()) {
            runtime_error("Operands must be numbers");
            RETURN_RUNTIME_ERROR;
        }
        S(a) = Value(rb.get_number() * rc.get_number());
        DISPATCH();
    }
    CASE(DIV): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (!rb.is_number() || !rc.is_number()) {
            runtime_error("Operands must be numbers");
            RETURN_RUNTIME_ERROR;
        }
        double divisor = rc.get_number();
        if (divisor == 0) {
            runtime_error("Division by zero");
            RETURN_RUNTIME_ERROR;
        }
        S(a) = Value(rb.get_number() / divisor);
        DISPATCH();
    }
    CASE(MOD): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (!rb.is_number() || !rc.is_number()) {
            runtime_error("Operands must be numbers");
            RETURN_RUNTIME_ERROR;
        }
        double divisor = rc.get_number();
        if (divisor == 0) {
            runtime_error("Modulo by zero");
            RETURN_RUNTIME_ERROR;
        }
        S(a) = Value(std::fmod(rb.get_number(), divisor));
        DISPATCH();
    }
    CASE(NEG): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        Value& rb = S(b);
        if (!rb.is_number()) {
            runtime_error("Operand must be a number");
            RETURN_RUNTIME_ERROR;
        }
        S(a) = Value(-rb.get_number());
        DISPATCH();
    }
    CASE(EQ): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        S(a) = Value(S(b) == S(c));
        DISPATCH();
    }
    CASE(NEQ): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        S(a) = Value(S(b) != S(c));
        DISPATCH();
    }
    CASE(LT): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() < rc.get_number());
        } else if (rb.is_string() && rc.is_string()) {
            S(a) = Value(rb.as_string()->value < rc.as_string()->value);
        } else {
            runtime_error("Operands must be numbers or strings");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(LTE): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() <= rc.get_number());
        } else if (rb.is_string() && rc.is_string()) {
            S(a) = Value(rb.as_string()->value <= rc.as_string()->value);
        } else {
            runtime_error("Operands must be numbers or strings");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(GT): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() > rc.get_number());
        } else if (rb.is_string() && rc.is_string()) {
            S(a) = Value(rb.as_string()->value > rc.as_string()->value);
        } else {
            runtime_error("Operands must be numbers or strings");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(GTE): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value& rb = S(b);
        Value& rc = S(c);
        if (rb.is_number() && rc.is_number()) {
            S(a) = Value(rb.get_number() >= rc.get_number());
        } else if (rb.is_string() && rc.is_string()) {
            S(a) = Value(rb.as_string()->value >= rc.as_string()->value);
        } else {
            runtime_error("Operands must be numbers or strings");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(NOT): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        S(a) = Value(!S(b).is_truthy());
        DISPATCH();
    }
    CASE(JMP): {
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        int16_t offset = static_cast<int16_t>(bx);
        frame->ip += offset * 4;
        DISPATCH();
    }
    CASE(JMP_IF_FALSE): {
        uint8_t a = frame->ip[1];
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        frame->ip += 4;
        if (!S(a).is_truthy()) {
            int16_t offset = static_cast<int16_t>(bx);
            frame->ip += offset * 4;
        }
        DISPATCH();
    }
    CASE(JMP_IF_TRUE): {
        uint8_t a = frame->ip[1];
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        frame->ip += 4;
        if (S(a).is_truthy()) {
            int16_t offset = static_cast<int16_t>(bx);
            frame->ip += offset * 4;
        }
        DISPATCH();
    }
    CASE(CALL): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        int arg_count = b;
        int callee_abs = base + a;
        Value callee = stack_[callee_abs];
        if (callee.is_closure()) {
            if (!call(callee.as_closure(), arg_count, a, callee_abs)) {
                RETURN_RUNTIME_ERROR;
            }
            REFRESH_FRAME();
        } else if (callee.is_native()) {
            if (!call_native(callee.as_native(), arg_count, a, callee_abs)) {
                RETURN_RUNTIME_ERROR;
            }
        } else if (callee.is_class()) {
            auto* instance = allocate_instance(callee.as_class());
            Value instance_val = Value(static_cast<Obj*>(instance));
            S(a) = instance_val;
            auto& methods = callee.as_class()->methods;
            auto init_it = methods.find("init");
            if (init_it == methods.end()) init_it = methods.find(akar_hash_symbol("init"));
            if (init_it != methods.end() && init_it->second.is_closure()) {
                for (int i = arg_count; i > 0; i--) {
                    stack_[callee_abs + 1 + i] = stack_[callee_abs + 1 + i - 1];
                }
                stack_[callee_abs + 1] = instance_val;
                stack_top_ += 1;
                if (!call(init_it->second.as_closure(), arg_count + 1, a, callee_abs)) {
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
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
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
            frame->ip = closure->function->bytecode.data();
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
            if (init_it == methods.end()) init_it = methods.find(akar_hash_symbol("init"));
            if (init_it != methods.end() && init_it->second.is_closure()) {
                for (int i = arg_count; i > 0; i--) {
                    stack_[callee_abs + 1 + i] = stack_[callee_abs + 1 + i - 1];
                }
                stack_[callee_abs + 1] = instance_val;
                stack_top_ += 1;
                auto* init_closure = init_it->second.as_closure();
                while (open_upvalues_ && open_upvalues_->location >= &stack_[base]) {
                    ObjUpvalue* uv = open_upvalues_;
                    uv->closed = *uv->location;
                    uv->location = &uv->closed;
                    open_upvalues_ = uv->next_upvalue;
                }
                int args_src = callee_abs + 1;
                int total_args = arg_count + 1;
                for (int i = 0; i < total_args; i++) {
                    stack_[base + i] = stack_[args_src + i];
                }
                int needed = init_closure->function->register_count;
                for (int i = total_args; i < needed; i++) {
                    stack_[base + i] = Value();
                }
                frame->closure = init_closure;
                frame->ip = init_closure->function->bytecode.data();
                stack_top_ = base + needed;
            }
        } else {
            runtime_error("Tail call target must be a function or class");
            RETURN_RUNTIME_ERROR;
        }
        DISPATCH();
    }
    CASE(CLOSURE): {
        uint8_t a = frame->ip[1];
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        frame->ip += 4;
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
        uint8_t a = frame->ip[1];
        frame->ip += 4;
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
        uint8_t a = frame->ip[1];
        frame->ip += 4;
        Value result = S(a);
        int callee_pos = frame->callee_stack_pos;
        int saved_top = frame->caller_stack_top;
        frame_count_--;
        if (frame_count_ == 0) {
            stack_top_ = 0;
            stack_[stack_top_++] = result;
            return InterpretResult::Ok;
        }
        // Restore caller's stack_top (preserving registers above callee_pos)
        stack_top_ = std::max(saved_top, callee_pos + 1);
        stack_[callee_pos] = result;
        REFRESH_FRAME();
        DISPATCH();
    }
    CASE(AWAIT): {
        uint8_t a = frame->ip[1];
        frame->ip += 4;
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
            break;
        }
        DISPATCH();
    }
    CASE(THROW): {
        uint8_t a = frame->ip[1];
        frame->ip += 4;
        Value exception = S(a);
        std::string msg;
        if (exception.is_string()) msg = exception.as_string()->value;
        else if (exception.is_number()) msg = std::to_string(exception.get_number());
        else msg = "thrown exception";
        if (try_count_ > 0) {
            TryFrame& tf = try_frames_[--try_count_];
            while (frame_count_ > tf.frame_count) {
                frame_count_--;
            }
            REFRESH_FRAME();
            frame->ip = tf.catch_ip;
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
        uint8_t a = frame->ip[1];
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        frame->ip += 4;
        if (try_count_ >= MAX_TRY) {
            runtime_error("Too many nested try blocks");
            RETURN_RUNTIME_ERROR;
        }
        TryFrame& tf = try_frames_[try_count_++];
        tf.frame_count = frame_count_;
        tf.catch_ip = frame->ip + bx * 4;
        tf.catch_register = a;
        DISPATCH();
    }
    CASE(TRY_END): {
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        frame->ip += 4;
        if (try_count_ > 0) try_count_--;
        if (bx > 0) {
            frame->ip += bx * 4;
        }
        DISPATCH();
    }
    CASE(NEW_ARRAY): {
        uint8_t a = frame->ip[1];
        frame->ip += 4;
        auto* arr = allocate_array();
        S(a) = Value(static_cast<Obj*>(arr));
        DISPATCH();
    }
    CASE(NEW_MAP): {
        uint8_t a = frame->ip[1];
        frame->ip += 4;
        auto* map = allocate_map();
        S(a) = Value(static_cast<Obj*>(map));
        DISPATCH();
    }
    CASE(GET_INDEX): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
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
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
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
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value obj = S(b);
        auto& constants = frame->closure->function->constants;
        if (c >= constants.size() || !constants[c].is_string()) {
            runtime_error("Invalid field name");
            RETURN_RUNTIME_ERROR;
        }
        std::string field = constants[c].as_string()->value;
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
        } else if (obj.is_string()) {
            if (field == "length" || field == akar_hash_symbol("length")) {
                S(a) = Value(static_cast<double>(obj.as_string()->value.size()));
            } else {
                S(a) = Value();
            }
        } else if (obj.is_array()) {
            if (field == "length" || field == akar_hash_symbol("length")) {
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
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
        Value obj = S(a);
        auto& constants = frame->closure->function->constants;
        if (b >= constants.size() || !constants[b].is_string()) {
            runtime_error("Invalid field name");
            RETURN_RUNTIME_ERROR;
        }
        std::string field = constants[b].as_string()->value;
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
        uint8_t a = frame->ip[1];
        uint16_t bx = (frame->ip[2] << 8) | frame->ip[3];
        frame->ip += 4;
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
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        if (!S(b).is_class()) {
            runtime_error("Cannot instantiate non-class");
            RETURN_RUNTIME_ERROR;
        }
        auto* inst = allocate_instance(S(b).as_class());
        S(a) = Value(static_cast<Obj*>(inst));
        DISPATCH();
    }
    CASE(GET_METHOD): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
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
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
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
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        Value iterable = S(b);
        auto* iter = allocate_map();
        if (iterable.is_array()) {
            iter->entries["__type__"] = Value(static_cast<Obj*>(get_string_table().intern("array_iter")));
            iter->entries["__data__"] = iterable;
            iter->entries["__index__"] = Value(0.0);
            iter->entries["__done__"] = Value(false);
        } else if (iterable.is_map() && iterable.as_map()->entries.count("__type__") &&
                   iterable.as_map()->entries["__type__"].is_string() &&
                   iterable.as_map()->entries["__type__"].as_string()->value == "range") {
            double start_val = iterable.as_map()->entries["start"].get_number();
            double end_val = iterable.as_map()->entries["end"].get_number();
            iter->entries["__type__"] = Value(static_cast<Obj*>(get_string_table().intern("range_iter")));
            iter->entries["__current__"] = Value(start_val);
            iter->entries["__end__"] = Value(end_val);
            iter->entries["__step__"] = Value(start_val <= end_val ? 1.0 : -1.0);
            iter->entries["__done__"] = Value(false);
        } else if (iterable.is_string()) {
            iter->entries["__type__"] = Value(static_cast<Obj*>(get_string_table().intern("string_iter")));
            iter->entries["__data__"] = iterable;
            iter->entries["__index__"] = Value(0.0);
            iter->entries["__done__"] = Value(false);
        } else {
            runtime_error("Cannot iterate this value");
            RETURN_RUNTIME_ERROR;
        }
        S(a) = Value(static_cast<Obj*>(iter));
        DISPATCH();
    }
    CASE(ITER_NEXT): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        Value iter_val = S(b);
        if (!iter_val.is_map()) {
            runtime_error("Invalid iterator");
            RETURN_RUNTIME_ERROR;
        }
        auto* iter = iter_val.as_map();
        std::string type = iter->entries["__type__"].as_string()->value;
        if (type == "array_iter") {
            int idx = static_cast<int>(iter->entries["__index__"].get_number());
            auto* arr = iter->entries["__data__"].as_array();
            if (idx < (int)arr->elements.size()) {
                S(a) = arr->elements[idx];
                iter->entries["__index__"] = Value(static_cast<double>(idx + 1));
            } else {
                iter->entries["__done__"] = Value(true);
                S(a) = Value();
            }
        } else if (type == "range_iter") {
            double current = iter->entries["__current__"].get_number();
            double end = iter->entries["__end__"].get_number();
            double step = iter->entries["__step__"].get_number();
            bool done = (step > 0) ? (current > end) : (current < end);
            if (!done) {
                S(a) = Value(current);
                iter->entries["__current__"] = Value(current + step);
            } else {
                iter->entries["__done__"] = Value(true);
                S(a) = Value();
            }
        } else if (type == "string_iter") {
            int idx = static_cast<int>(iter->entries["__index__"].get_number());
            auto& s = iter->entries["__data__"].as_string()->value;
            if (idx < (int)s.size()) {
                auto* ch = get_string_table().intern(std::string(1, s[idx]));
                S(a) = Value(static_cast<Obj*>(ch));
                iter->entries["__index__"] = Value(static_cast<double>(idx + 1));
            } else {
                iter->entries["__done__"] = Value(true);
                S(a) = Value();
            }
        }
        DISPATCH();
    }
    CASE(ITER_DONE): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        frame->ip += 4;
        Value iter = S(b);
        if (iter.is_map() && iter.as_map()->entries.count("__done__")) {
            S(a) = iter.as_map()->entries["__done__"];
        } else {
            S(a) = Value(true);
        }
        DISPATCH();
    }
    CASE(PRINT): {
        uint8_t a = frame->ip[1];
        frame->ip += 4;
        std::cout << S(a).to_string() << std::endl;
        DISPATCH();
    }
    CASE(HALT):
        return InterpretResult::Ok;
    CASE(NOP):
        frame->ip += 4;
        DISPATCH();
    CASE(FIBER_YIELD): {
        uint8_t a = frame->ip[1];
        frame->ip += 4;
        Value yielded = S(a);
        if (frame_count_ > 0) {
            int callee_pos = frame->callee_stack_pos;
            frame_count_--;
            if (frame_count_ == 0) {
                stack_top_ = 0;
                stack_[stack_top_++] = yielded;
                return InterpretResult::Ok;
            }
            stack_top_ = callee_pos;
            stack_[stack_top_++] = yielded;
            REFRESH_FRAME();
        }
        DISPATCH();
    }
    CASE(FIBER_RESUME): {
        uint8_t a = frame->ip[1];
        uint8_t b = frame->ip[2];
        uint8_t c = frame->ip[3];
        frame->ip += 4;
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
        if (fiber->state == ObjFiber::State::Created) {
            fiber->state = ObjFiber::State::Running;
            int callee_abs = stack_top_;
            stack_[stack_top_++] = Value(static_cast<Obj*>(fiber->entry));
            stack_[stack_top_++] = resume_val;
            if (!call(fiber->entry, 1, a, callee_abs)) {
                RETURN_RUNTIME_ERROR;
            }
            REFRESH_FRAME();
        }
        S(a) = resume_val;
        DISPATCH();
    }

    } // end for(;;)

#else
    // Fallback: switch-based dispatch for non-GCC compilers
    for (;;) {
    loop_continue:
        CHECK_MEMORY_LIMIT();
        uint8_t instruction = frame->ip[0];
        Opcode op = static_cast<Opcode>(instruction);
        switch (op) {
            case Opcode::LOAD_CONST: {
                uint8_t a = frame->ip[1]; uint16_t bx = (frame->ip[2] << 8) | frame->ip[3]; frame->ip += 4;
                S(a) = frame->closure->function->constants[bx]; break;
            }
            case Opcode::LOAD_NIL: { uint8_t a = frame->ip[1]; frame->ip += 4; S(a) = Value(); break; }
            case Opcode::LOAD_TRUE: { uint8_t a = frame->ip[1]; frame->ip += 4; S(a) = Value(true); break; }
            case Opcode::LOAD_FALSE: { uint8_t a = frame->ip[1]; frame->ip += 4; S(a) = Value(false); break; }
            case Opcode::MOVE: { uint8_t a = frame->ip[1]; uint8_t b = frame->ip[2]; frame->ip += 4; S(a) = S(b); break; }
            case Opcode::HALT: return InterpretResult::Ok;
            case Opcode::NOP: frame->ip += 4; break;
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
    if (new_top > stack_top_) stack_top_ = new_top;

    return true;
}

bool VM::call_native(ObjNative* native, int arg_count, int return_reg, int callee_abs) {
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
        auto* frame = &frames_[i];
        auto* func = frame->closure->function;
        size_t offset = frame->ip - func->bytecode.data();
        fprintf(stderr, "  [line %d] in %s\n", func->line, func->name.c_str());
    }
    fprintf(stderr, "Error: %s\n", buf);
}

void VM::define_native(const std::string& name, NativeFn function) {
    auto* native = allocate_native(function, name);
    globals_[name] = Value(static_cast<Obj*>(native));
    // Also register with hashed name so release .ako files can find it
    auto* native2 = allocate_native(std::move(function), name);
    globals_[akar_hash_symbol(name)] = Value(static_cast<Obj*>(native2));
}

void VM::set_global(const std::string& name, Value value) {
    globals_[name] = value;
}

Value VM::get_global(const std::string& name) const {
    auto it = globals_.find(name);
    if (it != globals_.end()) return it->second;
    return Value();
}

} // namespace akar
