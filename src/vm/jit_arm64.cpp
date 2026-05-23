// Akar Script — AArch64 JIT Backend
// Implements JITBackend for ARM64 (AArch64) architecture.
// All instruction encoding is isolated here.

#include "akar/vm/jit.h"
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

namespace akar {

// ============================================================
// ARM64 instruction encoding helpers
// ============================================================

static inline uint32_t enc_movz(int rd, uint16_t imm16, int shift) {
    return 0xD2800000u | ((shift & 3u) << 21) | (static_cast<uint32_t>(imm16) << 5) | (rd & 0x1Fu);
}
static inline uint32_t enc_movk(int rd, uint16_t imm16, int shift) {
    return 0xF2800000u | ((shift & 3u) << 21) | (static_cast<uint32_t>(imm16) << 5) | (rd & 0x1Fu);
}
static inline uint32_t enc_ldr(int rt, int rn, int offset) {
    return 0xF9400000u | ((static_cast<uint32_t>(offset / 8) & 0xFFFu) << 10) | ((rn & 0x1Fu) << 5) | (rt & 0x1Fu);
}
static inline uint32_t enc_str(int rt, int rn, int offset) {
    return 0xF9000000u | ((static_cast<uint32_t>(offset / 8) & 0xFFFu) << 10) | ((rn & 0x1Fu) << 5) | (rt & 0x1Fu);
}
static inline uint32_t enc_ldr_d(int dt, int rn, int offset) {
    return 0xFD400000u | ((static_cast<uint32_t>(offset / 8) & 0xFFFu) << 10) | ((rn & 0x1Fu) << 5) | (dt & 0x1Fu);
}
static inline uint32_t enc_str_d(int dt, int rn, int offset) {
    return 0xFD000000u | ((static_cast<uint32_t>(offset / 8) & 0xFFFu) << 10) | ((rn & 0x1Fu) << 5) | (dt & 0x1Fu);
}
static inline uint32_t enc_fadd(int dd, int dn, int dm) {
    return 0x1E602800u | ((dm & 0x1Fu) << 16) | ((dn & 0x1Fu) << 5) | (dd & 0x1Fu);
}
static inline uint32_t enc_fsub(int dd, int dn, int dm) {
    return 0x1E603800u | ((dm & 0x1Fu) << 16) | ((dn & 0x1Fu) << 5) | (dd & 0x1Fu);
}
static inline uint32_t enc_fmul(int dd, int dn, int dm) {
    return 0x1E600800u | ((dm & 0x1Fu) << 16) | ((dn & 0x1Fu) << 5) | (dd & 0x1Fu);
}
static inline uint32_t enc_fdiv(int dd, int dn, int dm) {
    return 0x1E601800u | ((dm & 0x1Fu) << 16) | ((dn & 0x1Fu) << 5) | (dd & 0x1Fu);
}
static inline uint32_t enc_fneg(int dd, int dn) {
    return 0x1E614000u | ((dn & 0x1Fu) << 5) | (dd & 0x1Fu);
}
static inline uint32_t enc_fcmp(int dn, int dm) {
    return 0x1E602000u | ((dm & 0x1Fu) << 16) | ((dn & 0x1Fu) << 5);
}
static inline uint32_t enc_fmov_from_x(int dd, int xn) {
    return 0x9E670000u | ((xn & 0x1Fu) << 5) | (dd & 0x1Fu);
}
static inline uint32_t enc_fmov_to_x(int xd, int dn) {
    return 0x9E660000u | ((dn & 0x1Fu) << 5) | (xd & 0x1Fu);
}
static inline uint32_t enc_mov_reg(int rd, int rm) {
    return 0xAA0003E0u | ((rm & 0x1Fu) << 16) | (rd & 0x1Fu);
}
static inline uint32_t enc_cmp(int rn, int rm) {
    return 0xEB000000u | ((rm & 0x1Fu) << 16) | ((rn & 0x1Fu) << 5) | 31u;
}
static inline uint32_t enc_b(int32_t imm26) {
    return 0x14000000u | (static_cast<uint32_t>(imm26) & 0x03FFFFFFu);
}
static inline uint32_t enc_b_cond(int imm19, int cond) {
    return 0x54000000u | ((static_cast<uint32_t>(imm19) & 0x7FFFFu) << 5) | (cond & 0xFu);
}
static inline uint32_t enc_stp(int rt1, int rt2, int rn, int imm7) {
    return 0xA9000000u | ((static_cast<uint32_t>(imm7) & 0x7Fu) << 15) | ((rt2 & 0x1Fu) << 10) | ((rn & 0x1Fu) << 5) | (rt1 & 0x1Fu);
}
static inline uint32_t enc_ldp(int rt1, int rt2, int rn, int imm7) {
    return 0xA9400000u | ((static_cast<uint32_t>(imm7) & 0x7Fu) << 15) | ((rt2 & 0x1Fu) << 10) | ((rn & 0x1Fu) << 5) | (rt1 & 0x1Fu);
}
static inline uint32_t enc_blr(int rn) {
    return 0xD63F0000u | ((rn & 0x1Fu) << 5);
}

static constexpr uint32_t ENC_NOP  = 0xD503201Fu;
static constexpr uint32_t ENC_RET  = 0xD65F03C0u;
static constexpr uint32_t ENC_MOV_W0_0 = 0x52800000u; // MOV W0, #0
static constexpr uint32_t ENC_MOV_W0_1 = 0x52800020u; // MOV W0, #1

// ============================================================
// ARM64Backend
// ============================================================

class ARM64Backend : public JITBackend {
public:
    // Register assignments (callee-saved, preserved across C calls)
    static constexpr int R_STACK  = 19;  // X19 = &stack_[0]
    static constexpr int R_OUTPC  = 20;  // X20 = out_pc pointer
    static constexpr int R_BASE   = 21;  // X21 = &stack_[base]
    static constexpr int R_CONST  = 22;  // X22 = &constants[0]
    static constexpr int R_CALLEE = 23;  // X23 = callee_pos (for RETURN)
    static constexpr int R_CALLER = 24;  // X24 = caller_top (for RETURN)
    static constexpr int R_CLOSURE = 25; // X25 = closure pointer
    static constexpr int R_SMI_TAG = 26; // X26 = cached SMALLINT_TAG (0xFFF7000000000000)

    // Scratch (caller-saved)
    static constexpr int R_SCRATCH0 = 0;  // X0
    static constexpr int R_SCRATCH1 = 1;  // X1
    static constexpr int FR_SCRATCH0 = 0; // D0
    static constexpr int FR_SCRATCH1 = 1; // D1

    // Condition codes
    static constexpr int CC_EQ = 0x0;
    static constexpr int CC_NE = 0x1;
    static constexpr int CC_LT = 0xB;
    static constexpr int CC_LE = 0xD;
    static constexpr int CC_GT = 0xC;
    static constexpr int CC_GE = 0xA;

    // --- JITBackend interface ---

    void reset() override { code_.clear(); }
    size_t code_size() const override { return code_.size(); }

    int reg_stack()  const override { return R_STACK; }
    int reg_outpc()  const override { return R_OUTPC; }
    int reg_base()   const override { return R_BASE; }
    int reg_const()  const override { return R_CONST; }
    int reg_callee() const override { return R_CALLEE; }
    int reg_caller() const override { return R_CALLER; }
    int reg_closure() const override { return R_CLOSURE; }
    int scratch0()   const override { return R_SCRATCH0; }
    int scratch1()   const override { return R_SCRATCH1; }
    int fscratch0()  const override { return FR_SCRATCH0; }
    int fscratch1()  const override { return FR_SCRATCH1; }

    void emit_prologue() override {
        // Frame: [SP+0]=X29/X30, [SP+16]=X19/X20, [SP+32]=X21/X22, [SP+48]=X23/X24, [SP+64]=X25/X26
        emit32(0xD10183FFu); // SUB SP, SP, #96
        emit32(enc_stp(29, 30, 31, 0));
        emit32(enc_stp(19, 20, 31, 16));
        emit32(enc_stp(21, 22, 31, 32));
        emit32(enc_stp(23, 24, 31, 48));
        emit32(enc_stp(25, 26, 31, 64));
        emit32(enc_mov_reg(29, 31));

        // Args: X0=stack, W1=base, X2=out_pc, X3=constants, W4=callee_pos, W5=caller_top, X6=closure
        emit32(enc_mov_reg(R_STACK, 0));        // X19 = stack
        emit32(enc_mov_reg(R_OUTPC, 2));        // X20 = out_pc
        emit32(0x8B010E60u | (R_BASE & 0x1Fu)); // ADD X21, X19, X1, LSL #3
        emit32(enc_mov_reg(R_CONST, 3));        // X22 = constants
        emit32(enc_mov_reg(R_CALLEE, 4));       // X23 = callee_pos
        emit32(enc_mov_reg(R_CALLER, 5));       // X24 = caller_top
        emit32(enc_mov_reg(R_CLOSURE, 6));      // X25 = closure

        // Cache SMALLINT_TAG in X26 for fast small int re-tagging
        emit_load_imm64(R_SMI_TAG, 0xFFF7000000000000ULL);
    }

    void emit_epilogue() override {
        emit32(enc_ldp(25, 26, 31, 64));
        emit32(enc_ldp(23, 24, 31, 48));
        emit32(enc_ldp(21, 22, 31, 32));
        emit32(enc_ldp(19, 20, 31, 16));
        emit32(enc_ldp(29, 30, 31, 0));
        emit32(0x910183FFu); // ADD SP, SP, #96
        emit32(ENC_RET);
    }

    void emit_set_return(int value) override {
        if (value == 0)
            emit32(ENC_MOV_W0_0);
        else
            emit32(ENC_MOV_W0_1);
    }

    void emit_load_int(int dest, int base, int offset) override {
        emit32(enc_ldr(dest, base, offset));
    }

    void emit_store_int(int src, int base, int offset) override {
        emit32(enc_str(src, base, offset));
    }

    void emit_load_fp(int dest_fp, int base, int offset) override {
        emit32(enc_ldr_d(dest_fp, base, offset));
    }

    void emit_store_fp(int src_fp, int base, int offset) override {
        emit32(enc_str_d(src_fp, base, offset));
    }

    void emit_load_value_as_fp(int dest_fp, int base, int offset) override {
        // Load raw 64-bit bits into scratch int register
        emit32(enc_ldr(R_SCRATCH0, base, offset));
        // Call jit_to_double_bits(X0) → result in X0 as raw double bits
        emit_load_imm64(8, reinterpret_cast<uint64_t>(static_cast<int64_t(*)(int64_t)>(&jit_to_double_bits)));
        emit32(enc_blr(8));
        // Move raw double bits from X0 to FP register
        emit32(enc_fmov_from_x(dest_fp, R_SCRATCH0));
    }

    void emit_load_imm64(int dest, uint64_t imm) override {
        bool first = true;
        for (int i = 0; i < 4; i++) {
            uint16_t chunk = (imm >> (i * 16)) & 0xFFFF;
            if (chunk != 0) {
                if (first) { emit32(enc_movz(dest, chunk, i)); first = false; }
                else        { emit32(enc_movk(dest, chunk, i)); }
            }
        }
        if (first) emit32(enc_movz(dest, 0, 0));
    }

    void emit_mov(int dest, int src) override {
        emit32(enc_mov_reg(dest, src));
    }

    void emit_add(int dest, int src1, int src2) override {
        emit32(0x8B000000u | ((src2 & 0x1Fu) << 16) | ((src1 & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_add_imm(int dest, int src, uint64_t imm12) override {
        // ADD Xd, Xn, #imm12 (12-bit unsigned immediate)
        emit32(0x91000000u | ((static_cast<uint32_t>(imm12 & 0xFFF) & 0xFFFu) << 10) | ((src & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_sub(int dest, int src1, int src2) override {
        emit32(0xCB000000u | ((src2 & 0x1Fu) << 16) | ((src1 & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_and(int dest, int src1, int src2) override {
        emit32(0x8A000000u | ((src2 & 0x1Fu) << 16) | ((src1 & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_orr(int dest, int src1, int src2) override {
        emit32(0xAA000000u | ((src2 & 0x1Fu) << 16) | ((src1 & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_eor(int dest, int src1, int src2) override {
        emit32(0xCA000000u | ((src2 & 0x1Fu) << 16) | ((src1 & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_lsl(int dest, int src, int shift) override {
        // LSL #shift = UBFM Xd, Xn, #(64-shift), #(63-shift)
        emit32(0xD3400000u | (((64 - shift) & 0x3Fu) << 16) | (((63 - shift) & 0x3Fu) << 10) | ((src & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_lsr(int dest, int src, int shift) override {
        // LSR #shift = UBFM Xd, Xn, #shift, #63
        emit32(0xD3400000u | ((shift & 0x3Fu) << 16) | (63u << 10) | ((src & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_mvn(int dest, int src) override {
        emit32(0xAA2003E0u | ((src & 0x1Fu) << 16) | (dest & 0x1Fu));
    }

    void emit_sxtw(int dest, int src) override {
        emit32(0x93407C00u | ((src & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_asr_imm(int dest, int src, int shift) override {
        // SBFM Xd, Xn, #shift, #63 = ASR Xd, Xn, #shift
        emit32(0x9340FC00u | (((shift) & 0x3Fu) << 16) | ((src & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_mul(int dest, int src1, int src2) override {
        // MUL Xd, Xn, Xm = MADD Xd, Xn, Xm, XZR
        emit32(0x9B007C00u | ((src2 & 0x1Fu) << 16) | ((src1 & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_sdiv(int dest, int src1, int src2) override {
        // SDIV Xd, Xn, Xm
        emit32(0x9AC00C00u | ((src2 & 0x1Fu) << 16) | ((src1 & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_msub(int dest, int src1, int src2, int src3) override {
        // MSUB Xd, Xn, Xm, Xa → Rd = Ra - Rn * Rm
        // Here: dest = src3 - src1 * src2
        // Base: 0x9B008000 (o0=1 at bit 15 distinguishes MSUB from MADD)
        emit32(0x9B008000u | ((src2 & 0x1Fu) << 16) | ((src3 & 0x1Fu) << 10) | ((src1 & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    void emit_fadd(int d, int s1, int s2) override { emit32(enc_fadd(d, s1, s2)); }
    void emit_fsub(int d, int s1, int s2) override { emit32(enc_fsub(d, s1, s2)); }
    void emit_fmul(int d, int s1, int s2) override { emit32(enc_fmul(d, s1, s2)); }
    void emit_fdiv(int d, int s1, int s2) override { emit32(enc_fdiv(d, s1, s2)); }
    void emit_fneg(int d, int s) override { emit32(enc_fneg(d, s)); }
    void emit_fcmp(int s1, int s2) override { emit32(enc_fcmp(s1, s2)); }
    void emit_fmov_from_int(int fd, int si) override { emit32(enc_fmov_from_x(fd, si)); }
    void emit_fmov_to_int(int dest_int, int src_fp) override { emit32(enc_fmov_to_x(dest_int, src_fp)); }
    void emit_frintz(int dd, int dn) override {
        // FRINTZ Dd, Dn — round toward zero (truncate)
        emit32(0x1E65C000u | ((dn & 0x1Fu) << 5) | (dd & 0x1Fu));
    }

    void emit_cmp(int s1, int s2) override { emit32(enc_cmp(s1, s2)); }

    void emit_cmp_imm(int src, uint64_t imm) override {
        // SUBS XZR, Xn, #imm12 (12-bit unsigned immediate)
        emit32(0xF1000000u | ((static_cast<uint32_t>(imm) & 0xFFFu) << 10) | ((src & 0x1Fu) << 5) | 31u);
    }

    void emit_lsr_imm(int dest, int src, int shift) override {
        // UBFM Xd, Xn, #shift, #63
        emit32(0xD3400000u | ((shift & 0x3Fu) << 16) | (63u << 10) | ((src & 0x1Fu) << 5) | (dest & 0x1Fu));
    }

    int cond_eq() const override { return CC_EQ; }
    int cond_ne() const override { return CC_NE; }
    int cond_lt() const override { return CC_LT; }
    int cond_le() const override { return CC_LE; }
    int cond_gt() const override { return CC_GT; }
    int cond_ge() const override { return CC_GE; }

    size_t emit_branch(int32_t offset) override {
        size_t pos = code_.size();
        emit32(enc_b(offset / 4));
        return pos;
    }

    size_t emit_branch_cond(int cond, int32_t offset) override {
        size_t pos = code_.size();
        emit32(enc_b_cond(offset / 4, cond));
        return pos;
    }

    void emit_call_indirect(void* func_addr) override {
        // Load address into X8, then BLR X8
        emit_load_imm64(8, reinterpret_cast<uint64_t>(func_addr));
        emit32(enc_blr(8));
    }

    void patch_branch(size_t code_pos, size_t target_code_pos) override {
        int32_t offset = static_cast<int32_t>(target_code_pos) - static_cast<int32_t>(code_pos);

        uint32_t inst = static_cast<uint32_t>(code_[code_pos]) |
                        (static_cast<uint32_t>(code_[code_pos+1]) << 8) |
                        (static_cast<uint32_t>(code_[code_pos+2]) << 16) |
                        (static_cast<uint32_t>(code_[code_pos+3]) << 24);

        uint32_t op = (inst >> 24) & 0xFF;
        if ((op & 0xFC) == 0x14) {
            // B: imm26
            inst = (inst & 0xFC000000u) | (static_cast<uint32_t>(offset / 4) & 0x03FFFFFFu);
        } else if ((op & 0xFF) == 0x54) {
            // B.cond: imm19
            inst = (inst & 0xFF00001Fu) | ((static_cast<uint32_t>(offset / 4) & 0x7FFFFu) << 5);
        }

        std::memcpy(&code_[code_pos], &inst, 4);
    }

    JITCode* finalize() override {
        size_t page_size = 4096;
        size_t alloc_size = (code_.size() + page_size - 1) & ~(page_size - 1);

        void* mem = mmap(nullptr, alloc_size,
                         PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) return nullptr;

        std::memcpy(mem, code_.data(), code_.size());
        __builtin___clear_cache(static_cast<char*>(mem),
                                static_cast<char*>(mem) + alloc_size);

        auto* jit = new JITCode();
        jit->memory = static_cast<uint8_t*>(mem);
        jit->size = alloc_size;
        jit->entry = reinterpret_cast<jit_fn_t>(mem);
        return jit;
    }

private:
    void emit32(uint32_t inst) {
        code_.push_back(inst & 0xFF);
        code_.push_back((inst >> 8) & 0xFF);
        code_.push_back((inst >> 16) & 0xFF);
        code_.push_back((inst >> 24) & 0xFF);
    }

    std::vector<uint8_t> code_;
};

// ============================================================
// Factory
// ============================================================

std::unique_ptr<JITBackend> create_jit_backend() {
    switch (detect_jit_platform()) {
        case JITPlatform::ARM64:
            return std::make_unique<ARM64Backend>();
        default:
            return nullptr;
    }
}

} // namespace akar
