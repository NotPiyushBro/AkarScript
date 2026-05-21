// Akar Script — Baseline JIT Compiler for AArch64
// Translates bytecode to native ARM64 machine code for hot functions.
// Unsupported opcodes bail back to the interpreter.

#include "akar/vm/jit.h"
#include "akar/vm/vm.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace akar {

// ============================================================
// JITCode destructor
// ============================================================
JITCode::~JITCode() {
    if (memory) {
        munmap(memory, size);
        memory = nullptr;
    }
}

// ============================================================
// ARM64 Register Assignments (callee-saved, preserved across calls)
// ============================================================
// X19 = &stack_[0]       — VM stack base
// X20 = out_pc pointer   — where to write bail-out bytecode offset
// X21 = &stack_[base]    — current function's base address
// X22 = &constants[0]    — function's constant pool
// X0-X15 = scratch (caller-saved, no save needed)
// D0-D7  = FP scratch (caller-saved)

static constexpr int REG_STACK  = 19;
static constexpr int REG_OUTPC  = 20;
static constexpr int REG_BASE   = 21;
static constexpr int REG_CONST  = 22;

// Scratch integer registers
static constexpr int X0_R = 0, X1_R = 1;

// FP scratch registers
static constexpr int D0 = 0, D1 = 1;

// NaN-boxed bit patterns
static constexpr int64_t NIL_BITS   = 0x7FFC000000000000LL;
static constexpr int64_t FALSE_BITS = 0x7FF8000000000001LL;
static constexpr int64_t TRUE_BITS  = 0x7FF8000000000002LL;

// ============================================================
// JITCompiler: ARM64 Instruction Encoding
// ============================================================

JITCompiler::JITCompiler() {}
JITCompiler::~JITCompiler() {}

void JITCompiler::emit32(uint32_t inst) {
    code_.push_back(inst & 0xFF);
    code_.push_back((inst >> 8) & 0xFF);
    code_.push_back((inst >> 16) & 0xFF);
    code_.push_back((inst >> 24) & 0xFF);
}

void JITCompiler::emit_movz(int rd, uint16_t imm16, int shift) {
    uint32_t hw = shift & 0x3;
    emit32(0xD2800000u | (hw << 21) | (static_cast<uint32_t>(imm16) << 5) | (rd & 0x1F));
}

void JITCompiler::emit_movk(int rd, uint16_t imm16, int shift) {
    uint32_t hw = shift & 0x3;
    emit32(0xF2800000u | (hw << 21) | (static_cast<uint32_t>(imm16) << 5) | (rd & 0x1F));
}

void JITCompiler::emit_ldr(int rt, int rn, int offset) {
    uint32_t imm12 = static_cast<uint32_t>(offset / 8);
    emit32(0xF9400000u | (imm12 << 10) | ((rn & 0x1F) << 5) | (rt & 0x1F));
}

void JITCompiler::emit_str(int rt, int rn, int offset) {
    uint32_t imm12 = static_cast<uint32_t>(offset / 8);
    emit32(0xF9000000u | (imm12 << 10) | ((rn & 0x1F) << 5) | (rt & 0x1F));
}

void JITCompiler::emit_ldr_d(int dt, int rn, int offset) {
    uint32_t imm12 = static_cast<uint32_t>(offset / 8);
    emit32(0xFD400000u | (imm12 << 10) | ((rn & 0x1F) << 5) | (dt & 0x1F));
}

void JITCompiler::emit_str_d(int dt, int rn, int offset) {
    uint32_t imm12 = static_cast<uint32_t>(offset / 8);
    emit32(0xFD000000u | (imm12 << 10) | ((rn & 0x1F) << 5) | (dt & 0x1F));
}

void JITCompiler::emit_fmov_d_from_x(int dd, int xn) {
    emit32(0x9E670000u | ((xn & 0x1F) << 5) | (dd & 0x1F));
}

void JITCompiler::emit_fmov_x_from_d(int xd, int dn) {
    emit32(0x9E660000u | ((dn & 0x1F) << 5) | (xd & 0x1F));
}

void JITCompiler::emit_fadd(int dd, int dn, int dm) {
    emit32(0x1E602800u | ((dm & 0x1F) << 16) | ((dn & 0x1F) << 5) | (dd & 0x1F));
}
void JITCompiler::emit_fsub(int dd, int dn, int dm) {
    emit32(0x1E603800u | ((dm & 0x1F) << 16) | ((dn & 0x1F) << 5) | (dd & 0x1F));
}
void JITCompiler::emit_fmul(int dd, int dn, int dm) {
    emit32(0x1E600800u | ((dm & 0x1F) << 16) | ((dn & 0x1F) << 5) | (dd & 0x1F));
}
void JITCompiler::emit_fdiv(int dd, int dn, int dm) {
    emit32(0x1E601800u | ((dm & 0x1F) << 16) | ((dn & 0x1F) << 5) | (dd & 0x1F));
}

void JITCompiler::emit_fneg(int dd, int dn) {
    emit32(0x1E614000u | ((dn & 0x1F) << 5) | (dd & 0x1F));
}

void JITCompiler::emit_fcmp(int dn, int dm) {
    emit32(0x1E602000u | ((dm & 0x1F) << 16) | ((dn & 0x1F) << 5));
}

size_t JITCompiler::emit_b(int32_t offset) {
    size_t pos = code_.size();
    int32_t imm26 = offset / 4;
    emit32(0x14000000u | (static_cast<uint32_t>(imm26) & 0x03FFFFFFu));
    return pos;
}

size_t JITCompiler::emit_b_cond(int cond, int32_t offset) {
    size_t pos = code_.size();
    int32_t imm19 = offset / 4;
    emit32(0x54000000u | ((static_cast<uint32_t>(imm19) & 0x7FFFFu) << 5) | (cond & 0xF));
    return pos;
}

size_t JITCompiler::emit_cbz(int xn, int32_t offset) {
    size_t pos = code_.size();
    int32_t imm19 = offset / 4;
    emit32(0xB4000000u | ((static_cast<uint32_t>(imm19) & 0x7FFFFu) << 5) | (xn & 0x1F));
    return pos;
}

size_t JITCompiler::emit_cbnz(int xn, int32_t offset) {
    size_t pos = code_.size();
    int32_t imm19 = offset / 4;
    emit32(0xB5000000u | ((static_cast<uint32_t>(imm19) & 0x7FFFFu) << 5) | (xn & 0x1F));
    return pos;
}

void JITCompiler::emit_nop() { emit32(0xD503201Fu); }
void JITCompiler::emit_ret()  { emit32(0xD65F03C0u); }

void JITCompiler::emit_stp(int rt1, int rt2, int rn, int imm) {
    int32_t imm7 = imm / 8;
    emit32(0xA9000000u | ((static_cast<uint32_t>(imm7) & 0x7F) << 15) |
           ((rt2 & 0x1F) << 10) | ((rn & 0x1F) << 5) | (rt1 & 0x1F));
}

void JITCompiler::emit_ldp(int rt1, int rt2, int rn, int imm) {
    int32_t imm7 = imm / 8;
    emit32(0xA9400000u | ((static_cast<uint32_t>(imm7) & 0x7F) << 15) |
           ((rt2 & 0x1F) << 10) | ((rn & 0x1F) << 5) | (rt1 & 0x1F));
}

void JITCompiler::emit_mov(int rd, int rm) {
    emit32(0xAA0003E0u | ((rm & 0x1F) << 16) | (rd & 0x1F));
}

void JITCompiler::emit_load_imm64(int rd, uint64_t imm) {
    bool first = true;
    for (int i = 0; i < 4; i++) {
        uint16_t chunk = (imm >> (i * 16)) & 0xFFFF;
        if (chunk != 0) {
            if (first) { emit_movz(rd, chunk, i); first = false; }
            else        { emit_movk(rd, chunk, i); }
        }
    }
    if (first) emit_movz(rd, 0, 0);
}

void JITCompiler::emit_add(int rd, int rn, int rm) {
    emit32(0x8B000000u | ((rm & 0x1F) << 16) | ((rn & 0x1F) << 5) | (rd & 0x1F));
}

void JITCompiler::emit_sub(int rd, int rn, int rm) {
    emit32(0xCB000000u | ((rm & 0x1F) << 16) | ((rn & 0x1F) << 5) | (rd & 0x1F));
}

void JITCompiler::emit_cmp(int rn, int rm) {
    emit32(0xEB000000u | ((rm & 0x1F) << 16) | ((rn & 0x1F) << 5) | 31);
}

// ============================================================
// JIT Helpers — C functions callable from JIT code
// ============================================================

static double jit_fmod(double a, double b) {
    return std::fmod(a, b);
}

static int64_t jit_mod_eq_zero(double a, double b) {
    return std::fmod(a, b) == 0.0 ? TRUE_BITS : FALSE_BITS;
}

// BLR Xn (branch with link to register)
void JITCompiler::emit_blr(int rn) {
    emit32(0xD63F0000u | ((rn & 0x1F) << 5));
}

// Call a C function via X8 (clobbers X8, X30/LR)
// FP args in D0, D1; FP result in D0; int result in X0
void JITCompiler::emit_call_helper_fp(void* func_addr) {
    emit_load_imm64(8, reinterpret_cast<uint64_t>(func_addr)); // X8 = address
    emit_blr(8);  // BLR X8
}

void JITCompiler::emit_bailout(int pc) {
    // *out_pc = pc
    emit_load_imm64(X0_R, static_cast<uint64_t>(pc));
    emit_str(X0_R, REG_OUTPC, 0);
    // Return JITResult::Bailout (1)
    emit32(0x52800020u); // MOV W0, #1
    // Epilogue (reverse of prologue)
    emit_ldp(21, 22, 31, 32);   // LDP X21, X22, [SP, #32]
    emit_ldp(19, 20, 31, 16);   // LDP X19, X20, [SP, #16]
    emit_ldp(29, 30, 31, 0);    // LDP X29, X30, [SP, #0]
    emit32(0x9100C3FFu);         // ADD SP, SP, #48
    emit_ret();
}

// ============================================================
// JIT Compiler: Main Compilation Loop
// ============================================================

bool JITCompiler::compile_instruction(int& pc) {
    int start_pc = pc;

    if (pc + 4 > static_cast<int>(bytecode_.size())) {
        emit_bailout(pc);
        return false;
    }

    uint8_t op = bytecode_[pc];
    uint8_t a  = bytecode_[pc + 1];
    uint8_t b  = bytecode_[pc + 2];
    uint8_t c  = bytecode_[pc + 3];
    uint16_t bx = (static_cast<uint16_t>(b) << 8) | c;
    int16_t sbx = static_cast<int16_t>(bx);
    pc += 4;

    switch (static_cast<Opcode>(op)) {

    // --- Load ---

    case Opcode::LOAD_IMM: {
        double d = static_cast<double>(b);
        uint64_t bits;
        std::memcpy(&bits, &d, 8);
        emit_load_imm64(X0_R, bits);
        emit_str(X0_R, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::LOAD_CONST: {
        emit_ldr(X0_R, REG_CONST, bx * 8);
        emit_str(X0_R, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::LOAD_NIL: {
        emit_load_imm64(X0_R, static_cast<uint64_t>(NIL_BITS));
        emit_str(X0_R, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::LOAD_TRUE: {
        emit_load_imm64(X0_R, static_cast<uint64_t>(TRUE_BITS));
        emit_str(X0_R, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::LOAD_FALSE: {
        emit_load_imm64(X0_R, static_cast<uint64_t>(FALSE_BITS));
        emit_str(X0_R, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::MOVE: {
        emit_ldr(X0_R, REG_BASE, stack_offset(b));
        emit_str(X0_R, REG_BASE, stack_offset(a));
        break;
    }

    // --- Arithmetic (type-specialized, direct FP) ---

    case Opcode::ADD_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fadd(D0, D0, D1);
        emit_str_d(D0, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::SUB_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fsub(D0, D0, D1);
        emit_str_d(D0, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::MUL_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fmul(D0, D0, D1);
        emit_str_d(D0, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::DIV_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fdiv(D0, D0, D1);
        emit_str_d(D0, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::MOD_NUM: {
        // Call jit_fmod(double, double) → double
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_call_helper_fp(reinterpret_cast<void*>(&jit_fmod));
        emit_str_d(D0, REG_BASE, stack_offset(a));
        break;
    }

    // --- ADD_IMM (fused LOAD_IMM + ADD) ---
    case Opcode::ADD_IMM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        double imm_d = static_cast<double>(c);
        uint64_t imm_bits;
        std::memcpy(&imm_bits, &imm_d, 8);
        emit_load_imm64(X0_R, imm_bits);
        emit_fmov_d_from_x(D1, X0_R);
        emit_fadd(D0, D0, D1);
        emit_str_d(D0, REG_BASE, stack_offset(a));
        break;
    }

    // --- MOD_EQ_ZERO: call helper, store bool result ---
    case Opcode::MOD_EQ_ZERO: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_call_helper_fp(reinterpret_cast<void*>(&jit_mod_eq_zero));
        // Result is int64_t NaN-boxed bool in X0
        emit_str(X0_R, REG_BASE, stack_offset(a));
        break;
    }

    // --- Generic arithmetic: bail ---
    case Opcode::ADD: case Opcode::SUB:
    case Opcode::MUL: case Opcode::DIV: case Opcode::MOD:
        emit_bailout(start_pc);
        return false;

    // --- Comparisons (type-specialized) ---

    case Opcode::LT_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0xB, 0);  // B.LT
        emit_load_imm64(X0_R, static_cast<uint64_t>(FALSE_BITS));
        size_t skip = emit_b(0);
        size_t true_pos = code_.size();
        emit_load_imm64(X0_R, static_cast<uint64_t>(TRUE_BITS));
        size_t store_pos = code_.size();
        emit_str(X0_R, REG_BASE, stack_offset(a));
        patch_branch(br, true_pos);
        patch_branch(skip, store_pos);
        break;
    }

    case Opcode::LTE_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0xD, 0);  // B.LE
        emit_load_imm64(X0_R, static_cast<uint64_t>(FALSE_BITS));
        size_t skip = emit_b(0);
        size_t true_pos = code_.size();
        emit_load_imm64(X0_R, static_cast<uint64_t>(TRUE_BITS));
        size_t store_pos = code_.size();
        emit_str(X0_R, REG_BASE, stack_offset(a));
        patch_branch(br, true_pos);
        patch_branch(skip, store_pos);
        break;
    }

    case Opcode::GT_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0xC, 0);  // B.GT
        emit_load_imm64(X0_R, static_cast<uint64_t>(FALSE_BITS));
        size_t skip = emit_b(0);
        size_t true_pos = code_.size();
        emit_load_imm64(X0_R, static_cast<uint64_t>(TRUE_BITS));
        size_t store_pos = code_.size();
        emit_str(X0_R, REG_BASE, stack_offset(a));
        patch_branch(br, true_pos);
        patch_branch(skip, store_pos);
        break;
    }

    case Opcode::GTE_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0xA, 0);  // B.GE
        emit_load_imm64(X0_R, static_cast<uint64_t>(FALSE_BITS));
        size_t skip = emit_b(0);
        size_t true_pos = code_.size();
        emit_load_imm64(X0_R, static_cast<uint64_t>(TRUE_BITS));
        size_t store_pos = code_.size();
        emit_str(X0_R, REG_BASE, stack_offset(a));
        patch_branch(br, true_pos);
        patch_branch(skip, store_pos);
        break;
    }

    case Opcode::EQ_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0x0, 0);  // B.EQ
        emit_load_imm64(X0_R, static_cast<uint64_t>(FALSE_BITS));
        size_t skip = emit_b(0);
        size_t true_pos = code_.size();
        emit_load_imm64(X0_R, static_cast<uint64_t>(TRUE_BITS));
        size_t store_pos = code_.size();
        emit_str(X0_R, REG_BASE, stack_offset(a));
        patch_branch(br, true_pos);
        patch_branch(skip, store_pos);
        break;
    }

    case Opcode::NEQ_NUM: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_ldr_d(D1, REG_BASE, stack_offset(c));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0x1, 0);  // B.NE
        emit_load_imm64(X0_R, static_cast<uint64_t>(FALSE_BITS));
        size_t skip = emit_b(0);
        size_t true_pos = code_.size();
        emit_load_imm64(X0_R, static_cast<uint64_t>(TRUE_BITS));
        size_t store_pos = code_.size();
        emit_str(X0_R, REG_BASE, stack_offset(a));
        patch_branch(br, true_pos);
        patch_branch(skip, store_pos);
        break;
    }

    // --- Generic comparisons: bail ---
    case Opcode::EQ: case Opcode::NEQ:
    case Opcode::LT: case Opcode::LTE:
    case Opcode::GT: case Opcode::GTE:
        emit_bailout(start_pc);
        return false;

    // --- Unary ---

    case Opcode::NEG: {
        emit_ldr_d(D0, REG_BASE, stack_offset(b));
        emit_fneg(D0, D0);
        emit_str_d(D0, REG_BASE, stack_offset(a));
        break;
    }

    case Opcode::NOT:
        emit_bailout(start_pc);
        return false;

    // --- Branches ---

    case Opcode::JMP: {
        // Interpreter's JMP applies offset from the instruction itself (not +4),
        // so target = start_pc + sbx (not pc + sbx)
        int target_pc = start_pc + sbx;
        if (target_pc < 0 || target_pc >= static_cast<int>(bytecode_.size())) {
            emit_bailout(start_pc);
            return false;
        }
        size_t branch_pos = emit_b(0);
        fixups_.push_back({branch_pos, target_pc, 0});
        break;
    }

    case Opcode::JMP_IF_FALSE: {
        // Fast path for boolean/comparison results: check against FALSE_VAL and NIL_VAL
        int target_pc = pc + sbx;
        emit_ldr(X0_R, REG_BASE, stack_offset(a));
        // Check FALSE_VAL (0x7FF8000000000001)
        emit_load_imm64(X1_R, static_cast<uint64_t>(FALSE_BITS));
        emit_cmp(X0_R, X1_R);
        size_t false_branch = emit_b_cond(0x0, 0);  // B.EQ → jump if false
        // Check NIL_VAL (0x7FFC000000000000)
        emit_load_imm64(X1_R, static_cast<uint64_t>(NIL_BITS));
        emit_cmp(X0_R, X1_R);
        size_t nil_branch = emit_b_cond(0x0, 0);  // B.EQ → jump if nil
        // Truthy: fall through
        fixups_.push_back({false_branch, target_pc, 1});
        fixups_.push_back({nil_branch, target_pc, 1});
        break;
    }

    case Opcode::JMP_IF_TRUE:
        emit_bailout(start_pc);
        return false;

    // --- Fused compare-branch ---

    case Opcode::JMP_IF_NOT_LT: {
        int target_pc = pc + static_cast<int8_t>(c);
        emit_ldr_d(D0, REG_BASE, stack_offset(a));
        emit_ldr_d(D1, REG_BASE, stack_offset(b));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0xA, 0);  // B.GE (not LT)
        fixups_.push_back({br, target_pc, 1});
        break;
    }

    case Opcode::JMP_IF_NOT_LTE: {
        int target_pc = pc + static_cast<int8_t>(c);
        emit_ldr_d(D0, REG_BASE, stack_offset(a));
        emit_ldr_d(D1, REG_BASE, stack_offset(b));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0xC, 0);  // B.GT (not LTE)
        fixups_.push_back({br, target_pc, 1});
        break;
    }

    case Opcode::JMP_IF_NOT_GT: {
        int target_pc = pc + static_cast<int8_t>(c);
        emit_ldr_d(D0, REG_BASE, stack_offset(a));
        emit_ldr_d(D1, REG_BASE, stack_offset(b));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0xD, 0);  // B.LE (not GT)
        fixups_.push_back({br, target_pc, 1});
        break;
    }

    case Opcode::JMP_IF_NOT_GTE: {
        int target_pc = pc + static_cast<int8_t>(c);
        emit_ldr_d(D0, REG_BASE, stack_offset(a));
        emit_ldr_d(D1, REG_BASE, stack_offset(b));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0xB, 0);  // B.LT (not GTE)
        fixups_.push_back({br, target_pc, 1});
        break;
    }

    case Opcode::JMP_IF_NOT_EQ: {
        int target_pc = pc + static_cast<int8_t>(c);
        emit_ldr_d(D0, REG_BASE, stack_offset(a));
        emit_ldr_d(D1, REG_BASE, stack_offset(b));
        emit_fcmp(D0, D1);
        size_t br = emit_b_cond(0x1, 0);  // B.NE (not EQ)
        fixups_.push_back({br, target_pc, 1});
        break;
    }

    // --- Return: bail (interpreter handles frame management) ---
    case Opcode::RETURN:
        emit_bailout(start_pc);
        return false;

    // --- Everything else: bail ---
    default:
        emit_bailout(start_pc);
        return false;
    }

    return true;
}

void JITCompiler::patch_branch(size_t code_pos, size_t target_code_pos) {
    int32_t offset = static_cast<int32_t>(target_code_pos) - static_cast<int32_t>(code_pos);

    uint32_t inst = static_cast<uint32_t>(code_[code_pos]) |
                    (static_cast<uint32_t>(code_[code_pos+1]) << 8) |
                    (static_cast<uint32_t>(code_[code_pos+2]) << 16) |
                    (static_cast<uint32_t>(code_[code_pos+3]) << 24);

    uint32_t op = (inst >> 24) & 0xFF;
    if ((op & 0xFC) == 0x14) {
        // Unconditional B: imm26
        int32_t imm26 = offset / 4;
        inst = (inst & 0xFC000000u) | (static_cast<uint32_t>(imm26) & 0x03FFFFFFu);
    } else if ((op & 0xFF) == 0x54) {
        // B.cond: imm19
        int32_t imm19 = offset / 4;
        inst = (inst & 0xFF00001Fu) | ((static_cast<uint32_t>(imm19) & 0x7FFFFu) << 5);
    } else if ((op & 0xFE) == 0xB4) {
        // CBZ/CBNZ: imm19
        int32_t imm19 = offset / 4;
        inst = (inst & 0xFF00001Fu) | ((static_cast<uint32_t>(imm19) & 0x7FFFFu) << 5);
    }

    code_[code_pos]     = inst & 0xFF;
    code_[code_pos + 1] = (inst >> 8) & 0xFF;
    code_[code_pos + 2] = (inst >> 16) & 0xFF;
    code_[code_pos + 3] = (inst >> 24) & 0xFF;
}

void JITCompiler::fixup_jumps() {
    for (auto& fixup : fixups_) {
        if (fixup.bc_target < 0 || fixup.bc_target / 4 >= static_cast<int>(bc_to_code_.size())) continue;
        int target_code = bc_to_code_[fixup.bc_target / 4];
        if (target_code < 0) continue;
        patch_branch(fixup.code_offset, static_cast<size_t>(target_code));
    }
    fixups_.clear();
}

JITCode* JITCompiler::compile(ObjFunction* function) {
    if (!function || function->bytecode.empty()) return nullptr;

    // Minimum function size to be worth JIT'ing
    if (function->bytecode.size() < 16) return nullptr;

    code_.clear();
    fixups_.clear();
    bytecode_ = function->bytecode;
    constants_ = function->constants;

    int total_bc = static_cast<int>(bytecode_.size());
    int total_inst = total_bc / 4;
    bc_to_code_.assign(total_inst + 1, -1);

    // --- Prologue ---
    // Allocate stack frame for callee-saved registers:
    //   [SP+0]  = X29 (FP)
    //   [SP+8]  = X30 (LR)
    //   [SP+16] = X19
    //   [SP+24] = X20
    //   [SP+32] = X21
    //   [SP+40] = X22
    // Total: 48 bytes

    // SUB SP, SP, #48
    emit32(0xD100C3FFu);
    // STP X29, X30, [SP, #0]
    emit_stp(29, 30, 31, 0);
    // STP X19, X20, [SP, #16]
    emit_stp(19, 20, 31, 16);
    // STP X21, X22, [SP, #32]
    emit_stp(21, 22, 31, 32);
    // MOV X29, SP
    emit_mov(29, 31);

    // Set up JIT registers from function arguments:
    //   X0 = stack (Value*)
    //   W1 = base (int)
    //   X2 = out_pc (int*)
    //   X3 = constants (Value*)
    emit_mov(REG_STACK, 0);          // X19 = stack
    emit_mov(REG_OUTPC, 2);          // X20 = out_pc
    // ADD X21, X19, X1, LSL #3      // X21 = &stack[base]
    emit32(0x8B010E60u | (21 & 0x1F)); // ADD X21, X19, X1, LSL #3
    emit_mov(REG_CONST, 3);          // X22 = constants

    // --- Compile bytecode ---
    int pc = 0;
    while (pc < total_bc) {
        bc_to_code_[pc / 4] = static_cast<int>(code_.size());
        if (!compile_instruction(pc)) {
            // Bailout was emitted, but keep going to compile the rest
            // (subsequent instructions will also bail, which is fine)
        }
    }

    // --- End-of-function bailout (in case we fall through) ---
    emit_bailout(total_bc);

    // --- Fix up jumps ---
    fixup_jumps();

    // --- Allocate executable memory ---
    size_t page_size = 4096;
    size_t alloc_size = (code_.size() + page_size - 1) & ~(page_size - 1);

    void* mem = mmap(nullptr, alloc_size,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("JIT: mmap failed");
        return nullptr;
    }

    std::memcpy(mem, code_.data(), code_.size());

    __builtin___clear_cache(static_cast<char*>(mem),
                            static_cast<char*>(mem) + alloc_size);

    auto* jit = new JITCode();
    jit->memory = static_cast<uint8_t*>(mem);
    jit->size = alloc_size;
    jit->entry = reinterpret_cast<jit_fn_t>(mem);
    return jit;
}

// ============================================================
// JITCache
// ============================================================

JITCode* JITCache::get_or_compile(ObjFunction* func) {
    auto it = compiled.find(func);
    if (it != compiled.end()) return it->second;

    JITCode* code = compiler.compile(func);
    if (code) {
        compiled[func] = code;
    }
    return code;
}

void JITCache::record_call(ObjFunction* func) {
    call_counts[func]++;
}

JITCache::~JITCache() {
    for (auto& [func, code] : compiled) {
        delete code;
    }
    compiled.clear();
}

} // namespace akar
