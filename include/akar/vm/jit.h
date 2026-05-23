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
// Args: stack, base, out_pc, constants, callee_pos, caller_top, closure
typedef JITResult (*jit_fn_t)(Value* stack, int base, int* out_pc, Value* constants,
                              int callee_pos, int caller_top, ObjClosure* closure);

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

class JITBackend {
public:
    virtual ~JITBackend() = default;

    // --- Code buffer ---
    virtual void   reset() = 0;
    virtual size_t code_size() const = 0;

    // --- Register accessors ---
    virtual int reg_stack()  const = 0;
    virtual int reg_outpc()  const = 0;
    virtual int reg_base()   const = 0;
    virtual int reg_const()  const = 0;
    virtual int reg_callee() const = 0;
    virtual int reg_caller() const = 0;
    virtual int reg_closure() const = 0;

    // Scratch registers (caller-saved, freely clobberable)
    virtual int scratch0()   const = 0;
    virtual int scratch1()   const = 0;
    virtual int fscratch0()  const = 0;
    virtual int fscratch1()  const = 0;

    // Stack slot size (8 for NaN-boxed Value)
    virtual int slot_size()  const { return 8; }

    // --- Prologue / Epilogue ---
    virtual void emit_prologue() = 0;
    virtual void emit_epilogue() = 0;
    virtual void emit_set_return(int value) = 0;

    // --- Load / Store ---
    virtual void emit_load_int(int dest, int base, int offset) = 0;
    virtual void emit_store_int(int src, int base, int offset) = 0;
    virtual void emit_load_fp(int dest_fp, int base, int offset) = 0;
    virtual void emit_store_fp(int src_fp, int base, int offset) = 0;
    // Load a NaN-boxed value and convert to double in FP register.
    // Handles small ints (tag 0xFFF7...), regular doubles, and NaN-boxed doubles.
    virtual void emit_load_value_as_fp(int dest_fp, int base, int offset) = 0;
    virtual void emit_load_imm64(int dest, uint64_t imm) = 0;

    // --- Move ---
    virtual void emit_mov(int dest, int src) = 0;

    // --- Integer arithmetic ---
    virtual void emit_add(int dest, int src1, int src2) = 0;
    virtual void emit_sub(int dest, int src1, int src2) = 0;
    virtual void emit_and(int dest, int src1, int src2) = 0;
    virtual void emit_orr(int dest, int src1, int src2) = 0;
    virtual void emit_eor(int dest, int src1, int src2) = 0;
    virtual void emit_lsl(int dest, int src, int shift) = 0;
    virtual void emit_lsr(int dest, int src, int shift) = 0;
    virtual void emit_mvn(int dest, int src) = 0;
    virtual void emit_sxtw(int dest, int src) = 0;
    // Arithmetic shift right by immediate (sign-extending)
    virtual void emit_asr_imm(int dest, int src, int shift) = 0;
    // 64-bit integer multiply: dest = src1 * src2
    virtual void emit_mul(int dest, int src1, int src2) = 0;
    // Signed integer divide: dest = src1 / src2
    virtual void emit_sdiv(int dest, int src1, int src2) = 0;
    // Multiply-subtract: dest = src3 - src1 * src2
    virtual void emit_msub(int dest, int src1, int src2, int src3) = 0;

    // --- FP Arithmetic ---
    virtual void emit_fadd(int dest, int src1, int src2) = 0;
    virtual void emit_fsub(int dest, int src1, int src2) = 0;
    virtual void emit_fmul(int dest, int src1, int src2) = 0;
    virtual void emit_fdiv(int dest, int src1, int src2) = 0;
    virtual void emit_fneg(int dest, int src) = 0;
    virtual void emit_fcmp(int src1, int src2) = 0;
    virtual void emit_fmov_from_int(int dest_fp, int src_int) = 0;
    virtual void emit_fmov_to_int(int dest_int, int src_fp) = 0;
    // Truncate FP toward zero (FRINTZ): dest_fp = trunc(src_fp)
    virtual void emit_frintz(int dest_fp, int src_fp) = 0;

    // --- Integer comparison ---
    virtual void emit_cmp(int src1, int src2) = 0;
    // Compare register with 12-bit immediate (sets flags)
    virtual void emit_cmp_imm(int src, uint64_t imm) = 0;
    // Logical shift right by immediate
    virtual void emit_lsr_imm(int dest, int src, int shift) = 0;

    // --- Condition codes ---
    virtual int cond_eq() const = 0;
    virtual int cond_ne() const = 0;
    virtual int cond_lt() const = 0;
    virtual int cond_le() const = 0;
    virtual int cond_gt() const = 0;
    virtual int cond_ge() const = 0;

    // --- Branches ---
    virtual size_t emit_branch(int32_t offset = 0) = 0;
    virtual size_t emit_branch_cond(int cond, int32_t offset = 0) = 0;

    // --- Function call ---
    virtual void emit_call_indirect(void* func_addr) = 0;

    // --- Patching ---
    virtual void patch_branch(size_t code_pos, size_t target_code_pos) = 0;

    // --- Finalize ---
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

std::unique_ptr<JITBackend> create_jit_backend();

// Set the VM pointer for JIT call helpers (must be called before JIT execution)
void jit_set_vm(VM* vm);

// Convert NaN-boxed bits to regular double bits (handles small ints).
// Returns raw double bits. If already a regular double, returns unchanged.
int64_t jit_to_double_bits(int64_t bits);

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

    // FP store cache: tracks last FP store to avoid redundant load after store
    struct FPStoreCache {
        bool valid = false;
        int slot_byte_offset = -1;  // byte offset into stack (slot_offset(reg))
        int fp_reg = -1;            // which FP register holds the value
    } fp_cache_;

    // Invalidate FP cache (call on jumps, helper calls, etc.)
    void invalidate_fp_cache() { fp_cache_.valid = false; }

    // Emit a bailout: save PC and return to interpreter
    void emit_bailout(int pc);

    // Emit a helper call: load address into scratch, BLR, result in X0
    void emit_helper_call(void* func_addr);

    // Fix up all pending jumps
    void fixup_jumps();

    // Type specialization: fast path mode (no guard checks)
    bool fast_path_ = false;
    bool is_integer_function() const;
};

// ============================================================
// JIT Cache — tracks hot functions, manages compiled code
// ============================================================

struct JITCache {
    std::unordered_map<ObjFunction*, JITCode*> compiled;
    std::unordered_map<ObjFunction*, int> call_counts;
    JITCompiler compiler;
    static constexpr int JIT_THRESHOLD = 50; // compile after 50 calls

    JITCode* get_or_compile(ObjFunction* func);
    void record_call(ObjFunction* func);
    ~JITCache();
};

} // namespace akar
