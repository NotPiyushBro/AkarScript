#pragma once
#include "akar/common/value.h"
#include "akar/common/opcodes.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <memory>

namespace akar {

class VM;
struct ObjFunction;

// ============================================================
// JIT compilation result
// ============================================================

enum class JITResult : uint8_t {
    Done = 0,      // function completed (RETURN hit)
    Bailout = 1,   // unsupported opcode, return to interpreter
};

// JIT'd function signature
// Args: stack, base, out_pc, constants, callee_pos, caller_top
// callee_pos: stack index to store return value
// caller_top: caller's stack_top for restoration after return
typedef JITResult (*jit_fn_t)(Value* stack, int base, int* out_pc, Value* constants,
                              int callee_pos, int caller_top);

// Compiled native code for a single function
struct JITCode {
    uint8_t* memory = nullptr;
    size_t   size = 0;
    jit_fn_t entry = nullptr;

    ~JITCode();
};

// ============================================================
// Abstract JIT Backend — platform-specific instruction encoding
// ============================================================
//
// Each backend (ARM64, x86-64, RISC-V, etc.) implements this interface.
// The JITCompiler uses only these methods — no platform-specific code
// in the compiler logic.
//
// Register model:
//   The backend allocates physical registers for these abstract roles:
//     REG_STACK  — pointer to VM stack base (&stack_[0])
//     REG_OUTPC  — pointer to int* for bail-out PC output
//     REG_BASE   — pointer to current function's registers (&stack_[base])
//     REG_CONST  — pointer to function's constant pool
//   Plus 2 scratch integer regs and 2 scratch FP regs.
//
// All offsets are in bytes. Stack slots are 8 bytes (sizeof(Value)).

class JITBackend {
public:
    virtual ~JITBackend() = default;

    // --- Code buffer ---

    virtual void   reset() = 0;
    virtual size_t code_size() const = 0;

    // --- Register accessors ---
    // Return physical register IDs for abstract roles.
    // These are callee-saved (preserved across C calls).
    virtual int reg_stack()  const = 0;
    virtual int reg_outpc()  const = 0;
    virtual int reg_base()   const = 0;
    virtual int reg_const()  const = 0;
    virtual int reg_callee() const = 0;  // callee_pos for RETURN
    virtual int reg_caller() const = 0;  // caller_top for RETURN

    // Scratch registers (caller-saved, freely clobberable)
    virtual int scratch0()   const = 0;
    virtual int scratch1()   const = 0;
    virtual int fscratch0()  const = 0;
    virtual int fscratch1()  const = 0;

    // Stack slot size (8 for NaN-boxed Value)
    virtual int slot_size()  const { return 8; }

    // --- Prologue / Epilogue ---
    // prologue: save callee-saved regs, set up REG_STACK/REG_OUTPC/REG_BASE/REG_CONST
    //   Args: X0=stack, W1=base, X2=out_pc, X3=constants  (AAPCS64 convention)
    virtual void emit_prologue() = 0;

    // epilogue: restore callee-saved regs, return W0 (JITResult)
    virtual void emit_epilogue() = 0;

    // Set return value (0=Done, 1=Bailout) before epilogue
    virtual void emit_set_return(int value) = 0;

    // --- Load / Store ---
    // dest = *(int64_t*)(base + offset)
    virtual void emit_load_int(int dest, int base, int offset) = 0;
    // *(int64_t*)(base + offset) = src
    virtual void emit_store_int(int src, int base, int offset) = 0;
    // dest_fp = *(double*)(base + offset)
    virtual void emit_load_fp(int dest_fp, int base, int offset) = 0;
    // *(double*)(base + offset) = src_fp
    virtual void emit_store_fp(int src_fp, int base, int offset) = 0;
    // dest = imm64
    virtual void emit_load_imm64(int dest, uint64_t imm) = 0;

    // --- Move ---
    virtual void emit_mov(int dest, int src) = 0;

    // --- Integer arithmetic ---
    virtual void emit_add(int dest, int src1, int src2) = 0;

    // --- FP Arithmetic ---
    // dest_fp = src1_fp + src2_fp  (and sub, mul, div)
    virtual void emit_fadd(int dest, int src1, int src2) = 0;
    virtual void emit_fsub(int dest, int src1, int src2) = 0;
    virtual void emit_fmul(int dest, int src1, int src2) = 0;
    virtual void emit_fdiv(int dest, int src1, int src2) = 0;
    // dest_fp = -src_fp
    virtual void emit_fneg(int dest, int src) = 0;
    // compare src1_fp vs src2_fp (sets CPU flags)
    virtual void emit_fcmp(int src1, int src2) = 0;
    // dest_fp = *(double*)&src_int  (bitwise reinterpret)
    virtual void emit_fmov_from_int(int dest_fp, int src_int) = 0;

    // --- Integer comparison ---
    virtual void emit_cmp(int src1, int src2) = 0;

    // --- Condition codes (return platform-specific values) ---
    virtual int cond_eq() const = 0;
    virtual int cond_ne() const = 0;
    virtual int cond_lt() const = 0;
    virtual int cond_le() const = 0;
    virtual int cond_gt() const = 0;
    virtual int cond_ge() const = 0;

    // --- Branches ---
    // Unconditional branch. Returns code position for later patching.
    virtual size_t emit_branch(int32_t offset = 0) = 0;
    // Conditional branch (uses last cmp/fcmp result). Returns position for patching.
    virtual size_t emit_branch_cond(int cond, int32_t offset = 0) = 0;

    // --- Function call ---
    // Load a 64-bit address into a scratch register and call it indirectly.
    // Clobbers scratch registers. C calling convention (args in D0/D1 or X0/X1).
    virtual void emit_call_indirect(void* func_addr) = 0;

    // --- Patching ---
    // Patch a branch at code_pos to jump to target_code_pos.
    virtual void patch_branch(size_t code_pos, size_t target_code_pos) = 0;

    // --- Finalize ---
    // Allocate executable memory, copy code, clear icache. Returns nullptr on failure.
    virtual JITCode* finalize() = 0;
};

// ============================================================
// Platform detection and backend factory
// ============================================================

enum class JITPlatform {
    ARM64,
    X86_64,
    Unknown,
};

inline JITPlatform detect_jit_platform() {
#if defined(__aarch64__)
    return JITPlatform::ARM64;
#elif defined(__x86_64__)
    return JITPlatform::X86_64;
#else
    return JITPlatform::Unknown;
#endif
}

// Create the appropriate backend for the current platform.
// Returns nullptr if the platform is not supported.
std::unique_ptr<JITBackend> create_jit_backend();

// Set the VM pointer for JIT call helpers (must be called before JIT execution)
void jit_set_vm(VM* vm);

// ============================================================
// JIT Compiler — platform-independent compilation logic
// ============================================================

class JITCompiler {
public:
    JITCompiler();
    ~JITCompiler();

    // Compile a function to native code. Returns nullptr on failure.
    JITCode* compile(ObjFunction* function);

    // Access the backend (for testing)
    JITBackend* backend() const { return backend_.get(); }

private:
    std::unique_ptr<JITBackend> backend_;

    // --- Bytecode offset → code offset mapping ---
    struct JumpFixup {
        size_t code_offset;
        int    bc_target;
        int    branch_type;   // 0=unconditional, 1=conditional
    };
    std::vector<JumpFixup> fixups_;
    std::vector<int> bc_to_code_;
    std::vector<uint8_t> bytecode_;
    std::vector<Value>   constants_;

    // Byte offset for VM stack slot `reg` relative to REG_BASE
    int slot_offset(int reg) const { return reg * backend_->slot_size(); }

    // Generate code for a single bytecode instruction
    bool compile_instruction(int& pc);

    // Emit a bailout: save PC and return to interpreter
    void emit_bailout(int pc);

    // Fix up all pending jumps
    void fixup_jumps();
};

// ============================================================
// JIT Cache — tracks hot functions, manages compiled code
// ============================================================

struct JITCache {
    std::unordered_map<ObjFunction*, JITCode*> compiled;
    std::unordered_map<ObjFunction*, int> call_counts;
    JITCompiler compiler;
    static constexpr int JIT_THRESHOLD = 100;

    JITCode* get_or_compile(ObjFunction* func);
    void record_call(ObjFunction* func);
    ~JITCache();
};

} // namespace akar
