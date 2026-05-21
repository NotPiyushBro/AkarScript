#pragma once
#include "akar/common/value.h"
#include "akar/common/opcodes.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <sys/mman.h>
#include <cstring>

namespace akar {

class VM;
struct ObjFunction;

// JIT compilation status
enum class JITResult : uint8_t {
    Done = 0,      // function completed (RETURN hit)
    Bailout = 1,   // unsupported opcode, return to interpreter
};

// JIT'd function signature
// Args: stack pointer, base register offset, out_pc (where to write bail PC), constants
// Returns: JITResult
typedef JITResult (*jit_fn_t)(Value* stack, int base, int* out_pc, Value* constants);

// Compiled native code for a single function
struct JITCode {
    uint8_t* memory;        // mmap'd executable memory
    size_t   size;          // bytes of generated code
    jit_fn_t entry;         // function pointer to call
    int      entry_offset;  // offset to entry point within memory

    JITCode() : memory(nullptr), size(0), entry(nullptr), entry_offset(0) {}
    ~JITCode();
};

// Baseline JIT compiler for AArch64
class JITCompiler {
public:
    JITCompiler();
    ~JITCompiler();

    // Compile a function. Returns nullptr if compilation fails.
    JITCode* compile(ObjFunction* function);

private:
    // --- ARM64 instruction encoding ---

    // Append a 32-bit instruction
    void emit32(uint32_t inst);

    // MOVZ Xd, #imm16, LSL #(shift*16)
    void emit_movz(int rd, uint16_t imm16, int shift = 0);

    // MOVK Xd, #imm16, LSL #(shift*16)
    void emit_movk(int rd, uint16_t imm16, int shift);

    // LDR Xt, [Xn, #offset] (unsigned, scaled by 8 for 64-bit)
    // offset must be 8-byte aligned and in range [0, 32760]
    void emit_ldr(int rt, int rn, int offset);

    // STR Xt, [Xn, #offset] (unsigned, scaled by 8 for 64-bit)
    void emit_str(int rt, int rn, int offset);

    // LDR Dt, [Xn, #offset] (FP, unsigned, scaled by 8)
    void emit_ldr_d(int dt, int rn, int offset);

    // STR Dt, [Xn, #offset] (FP, unsigned, scaled by 8)
    void emit_str_d(int dt, int rn, int offset);

    // FMOV Dd, Xn (general register → FP register)
    void emit_fmov_d_from_x(int dd, int xn);

    // FMOV Xd, Dn (FP register → general register)
    void emit_fmov_x_from_d(int xd, int dn);

    // FADD/FSUB/FMUL/FDIV Dd, Dn, Dm
    void emit_fadd(int dd, int dn, int dm);
    void emit_fsub(int dd, int dn, int dm);
    void emit_fmul(int dd, int dn, int dm);
    void emit_fdiv(int dd, int dn, int dm);

    // FNEG Dd, Dn
    void emit_fneg(int dd, int dn);

    // FCMP Dn, Dm
    void emit_fcmp(int dn, int dm);

    // FMADD Dd, Dn, Dm, Da (Dd = Da + Dn * Dm) — for fused ops
    void emit_fmadd(int dd, int dn, int dm, int da);

    // B #offset (unconditional, offset in bytes from current PC)
    // Returns the code position of the instruction (for fixup)
    size_t emit_b(int32_t offset = 0);

    // B.cond #offset
    // cond: 0=EQ, 1=NE, 2=CS, 3=CC, 4=MI, 5=PL, 6=VS, 7=VC,
    //       8=HI, 9=LS, 10=GE, 11=LT, 12=GT, 13=LE
    size_t emit_b_cond(int cond, int32_t offset = 0);

    // CBZ/CBNZ Xn, #offset
    size_t emit_cbz(int xn, int32_t offset = 0);
    size_t emit_cbnz(int xn, int32_t offset = 0);

    // NOP
    void emit_nop();

    // RET (X30)
    void emit_ret();

    // STP/LDP for saving/restoring callee-saved registers
    void emit_stp(int rt1, int rt2, int rn, int imm);
    void emit_ldp(int rt1, int rt2, int rn, int imm);

    // SUB SP, SP, #imm
    void emit_sub_sp(int imm);

    // ADD SP, SP, #imm
    void emit_add_sp(int imm);

    // MOV Xd, Xm (register move)
    void emit_mov(int rd, int rm);

    // Load 64-bit immediate into register (emits 1-4 MOVZ/MOVK instructions)
    void emit_load_imm64(int rd, uint64_t imm);

    // ADD Xd, Xn, Xm (64-bit)
    void emit_add(int rd, int rn, int rm);

    // SUB Xd, Xn, Xm (64-bit)
    void emit_sub(int rd, int rn, int rm);

    // CMP Xn, Xm (alias for SUBS XZR, Xn, Xm)
    void emit_cmp(int rn, int rm);

    // UBFM (for shifts) — not needed in baseline

    // --- JIT compilation ---

    // Bytecode offset → code offset mapping (for jump fixups)
    struct JumpFixup {
        size_t code_offset;   // where the branch instruction is
        int    bc_target;     // target bytecode offset (-1 = end of function)
        int    branch_type;   // 0=B, 1=B.cond (GE), 2=B.cond (LT), etc.
    };

    // Pending branch fixups
    std::vector<JumpFixup> fixups_;

    // Map from bytecode offset → code offset
    std::vector<int> bc_to_code_;

    // Current bytecode being compiled
    std::vector<uint8_t> bytecode_;
    std::vector<Value>   constants_;

    // Output code buffer
    std::vector<uint8_t> code_;

    // --- Register mapping ---
    // AArch64 callee-saved: X19-X28, D8-D15
    // We use:
    //   X19 = VM stack base (Value* stack_)
    //   X20 = out_pc pointer
    //   X21 = &stack_[base] (precomputed base address)
    //   X22 = &constants[0] (function's constant pool)
    //   D0-D7 = FP scratch (caller-saved, no save needed)
    //   X0-X15 = integer scratch (caller-saved)
    static constexpr int REG_STACK  = 19;  // X19 = &stack_[0]
    static constexpr int REG_OUTPC  = 20;  // X20 = out_pc pointer
    static constexpr int REG_BASE   = 21;  // X21 = &stack_[base]
    static constexpr int REG_CONST  = 22;  // X22 = &constants[0]

    // Offset of a VM stack slot from X19: stack_[i] is at [X19, #i*8]
    static constexpr int STACK_SLOT_SIZE = 8; // sizeof(Value) = 8 bytes

    // Helper: byte offset for stack slot `reg`
    int stack_offset(int reg) const { return reg * STACK_SLOT_SIZE; }

    // Generate code for a single bytecode instruction
    bool compile_instruction(int& pc);

    // Emit code to load Value from stack[base+src_reg] into FP register Dd
    void emit_load_value_to_fp(int dd, int vm_reg);

    // Emit code to store FP register Dd into stack[base+dst_reg]
    void emit_store_fp_to_value(int dd, int vm_reg);

    // Emit a bailout: save PC and return to interpreter
    void emit_bailout(int pc);

    // Patch a branch at `code_pos` to target `target_code_pos`
    void patch_branch(size_t code_pos, size_t target_code_pos);

    // Fix up all pending jumps after compilation
    void fixup_jumps();
};

// JIT cache: tracks which functions have been JIT'd
struct JITCache {
    std::unordered_map<ObjFunction*, JITCode*> compiled;
    std::unordered_map<ObjFunction*, int> call_counts;
    JITCompiler compiler;
    static constexpr int JIT_THRESHOLD = 100; // JIT after this many calls

    JITCode* get_or_compile(ObjFunction* func);
    void record_call(ObjFunction* func);
    ~JITCache();
};

} // namespace akar
