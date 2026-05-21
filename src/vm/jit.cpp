// Akar Script — Platform-independent JIT Compiler
// Uses JITBackend abstract interface for code generation.
// No platform-specific code here.

#include "akar/vm/jit.h"
#include "akar/vm/vm.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sys/mman.h>

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
// Helper functions called from JIT code
// ============================================================

static int64_t jit_fmod(double a, double b) {
    double r = std::fmod(a, b);
    uint64_t bits;
    std::memcpy(&bits, &r, 8);
    return static_cast<int64_t>(bits);
}

static int64_t jit_mod_eq_zero(double a, double b) {
    return std::fmod(a, b) == 0.0
        ? static_cast<int64_t>(0x7FF8000000000002LL)  // TRUE_BITS
        : static_cast<int64_t>(0x7FF8000000000001LL);  // FALSE_BITS
}

// NaN-boxed bit patterns
static constexpr int64_t NIL_BITS   = 0x7FFC000000000000LL;
static constexpr int64_t FALSE_BITS = 0x7FF8000000000001LL;
static constexpr int64_t TRUE_BITS  = 0x7FF8000000000002LL;

// ============================================================
// JITCompiler
// ============================================================

JITCompiler::JITCompiler() {
    backend_ = create_jit_backend();
}

JITCompiler::~JITCompiler() = default;

void JITCompiler::emit_bailout(int pc) {
    auto* b = backend_.get();
    // *out_pc = pc
    b->emit_load_imm64(b->scratch0(), static_cast<uint64_t>(pc));
    b->emit_store_int(b->scratch0(), b->reg_outpc(), 0);
    // Return Bailout
    b->emit_set_return(1);
    b->emit_epilogue();
}

bool JITCompiler::compile_instruction(int& pc) {
    auto* b = backend_.get();
    int start_pc = pc;

    if (pc + 4 > static_cast<int>(bytecode_.size())) {
        emit_bailout(pc);
        return false;
    }

    uint8_t op = bytecode_[pc];
    uint8_t a  = bytecode_[pc + 1];
    uint8_t b8  = bytecode_[pc + 2];
    uint8_t c8  = bytecode_[pc + 3];
    uint16_t bx = (static_cast<uint16_t>(b8) << 8) | c8;
    int16_t sbx = static_cast<int16_t>(bx);
    pc += 4;

    int R0 = b->scratch0();
    int R1 = b->scratch1();
    int D0 = b->fscratch0();
    int D1 = b->fscratch1();

    switch (static_cast<Opcode>(op)) {

    // --- Load ---

    case Opcode::LOAD_IMM: {
        double d = static_cast<double>(b8);
        uint64_t bits;
        std::memcpy(&bits, &d, 8);
        b->emit_load_imm64(R0, bits);
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::LOAD_CONST: {
        b->emit_load_int(R0, b->reg_const(), bx * 8);
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::LOAD_NIL: {
        b->emit_load_imm64(R0, static_cast<uint64_t>(NIL_BITS));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::LOAD_TRUE: {
        b->emit_load_imm64(R0, static_cast<uint64_t>(TRUE_BITS));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::LOAD_FALSE: {
        b->emit_load_imm64(R0, static_cast<uint64_t>(FALSE_BITS));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::MOVE: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    // --- Arithmetic (type-specialized) ---

    case Opcode::ADD_NUM: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8));
        b->emit_fadd(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::SUB_NUM: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8));
        b->emit_fsub(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::MUL_NUM: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8));
        b->emit_fmul(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::DIV_NUM: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8));
        b->emit_fdiv(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::MOD_NUM: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8));
        b->emit_call_indirect(reinterpret_cast<void*>(&jit_fmod));
        // Result in D0 (as raw bits via X0 → store as int, then reload as FP)
        // Actually jit_fmod returns int64_t bits in X0, store as int
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::ADD_IMM: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        double imm_d = static_cast<double>(c8);
        uint64_t imm_bits;
        std::memcpy(&imm_bits, &imm_d, 8);
        b->emit_load_imm64(R0, imm_bits);
        b->emit_fmov_from_int(D1, R0);
        b->emit_fadd(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::MOD_EQ_ZERO: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8));
        b->emit_call_indirect(reinterpret_cast<void*>(&jit_mod_eq_zero));
        // Result is NaN-boxed bool in X0
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    // --- Generic arithmetic: bail ---
    case Opcode::ADD: case Opcode::SUB:
    case Opcode::MUL: case Opcode::DIV: case Opcode::MOD:
        emit_bailout(start_pc);
        return false;

    // --- Comparisons (type-specialized) ---

    #define EMIT_CMP_BRANCH_STORE(CC) do { \
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8)); \
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8)); \
        b->emit_fcmp(D0, D1); \
        size_t br = b->emit_branch_cond(b->CC(), 0); \
        b->emit_load_imm64(R0, static_cast<uint64_t>(FALSE_BITS)); \
        size_t skip = b->emit_branch(0); \
        size_t true_pos = b->code_size(); \
        b->emit_load_imm64(R0, static_cast<uint64_t>(TRUE_BITS)); \
        size_t store_pos = b->code_size(); \
        b->emit_store_int(R0, b->reg_base(), slot_offset(a)); \
        b->patch_branch(br, true_pos); \
        b->patch_branch(skip, store_pos); \
    } while(0)

    case Opcode::LT_NUM:  EMIT_CMP_BRANCH_STORE(cond_lt); break;
    case Opcode::LTE_NUM: EMIT_CMP_BRANCH_STORE(cond_le); break;
    case Opcode::GT_NUM:  EMIT_CMP_BRANCH_STORE(cond_gt); break;
    case Opcode::GTE_NUM: EMIT_CMP_BRANCH_STORE(cond_ge); break;
    case Opcode::EQ_NUM:  EMIT_CMP_BRANCH_STORE(cond_eq); break;
    case Opcode::NEQ_NUM: EMIT_CMP_BRANCH_STORE(cond_ne); break;

    #undef EMIT_CMP_BRANCH_STORE

    case Opcode::EQ: case Opcode::NEQ:
    case Opcode::LT: case Opcode::LTE:
    case Opcode::GT: case Opcode::GTE:
        emit_bailout(start_pc);
        return false;

    // --- Unary ---

    case Opcode::NEG: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_fneg(D0, D0);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::NOT:
        emit_bailout(start_pc);
        return false;

    // --- Branches ---

    case Opcode::JMP: {
        // Interpreter's JMP: ip += offset (from instruction, NOT +4)
        int target_pc = start_pc + sbx;
        if (target_pc < 0 || target_pc >= static_cast<int>(bytecode_.size())) {
            emit_bailout(start_pc);
            return false;
        }
        size_t br = b->emit_branch(0);
        fixups_.push_back({br, target_pc, 0});
        break;
    }

    case Opcode::JMP_IF_FALSE: {
        int target_pc = pc + sbx;
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        // Check FALSE_VAL
        b->emit_load_imm64(R1, static_cast<uint64_t>(FALSE_BITS));
        b->emit_cmp(R0, R1);
        size_t false_br = b->emit_branch_cond(b->cond_eq(), 0);
        // Check NIL_VAL
        b->emit_load_imm64(R1, static_cast<uint64_t>(NIL_BITS));
        b->emit_cmp(R0, R1);
        size_t nil_br = b->emit_branch_cond(b->cond_eq(), 0);
        // Truthy: fall through
        fixups_.push_back({false_br, target_pc, 1});
        fixups_.push_back({nil_br, target_pc, 1});
        break;
    }

    case Opcode::JMP_IF_TRUE:
        emit_bailout(start_pc);
        return false;

    // --- Fused compare-branch ---
    // Interpreter advances ip by 4 first, then applies offset: target = (pc) + c8
    // pc is already advanced by 4 in this function

    #define EMIT_FUSED_CMP_BRANCH(CC) do { \
        int target_pc = pc + static_cast<int8_t>(c8); \
        b->emit_load_fp(D0, b->reg_base(), slot_offset(a)); \
        b->emit_load_fp(D1, b->reg_base(), slot_offset(b8)); \
        b->emit_fcmp(D0, D1); \
        size_t br = b->emit_branch_cond(b->CC(), 0); \
        fixups_.push_back({br, target_pc, 1}); \
    } while(0)

    case Opcode::JMP_IF_NOT_LT:  EMIT_FUSED_CMP_BRANCH(cond_ge); break;
    case Opcode::JMP_IF_NOT_LTE: EMIT_FUSED_CMP_BRANCH(cond_gt); break;
    case Opcode::JMP_IF_NOT_GT:  EMIT_FUSED_CMP_BRANCH(cond_le); break;
    case Opcode::JMP_IF_NOT_GTE: EMIT_FUSED_CMP_BRANCH(cond_lt); break;
    case Opcode::JMP_IF_NOT_EQ:  EMIT_FUSED_CMP_BRANCH(cond_ne); break;

    #undef EMIT_FUSED_CMP_BRANCH

    // --- Return: bail ---
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

void JITCompiler::fixup_jumps() {
    for (auto& fixup : fixups_) {
        if (fixup.bc_target < 0 || fixup.bc_target / 4 >= static_cast<int>(bc_to_code_.size()))
            continue;
        int target_code = bc_to_code_[fixup.bc_target / 4];
        if (target_code < 0) continue;
        backend_->patch_branch(fixup.code_offset, static_cast<size_t>(target_code));
    }
    fixups_.clear();
}

JITCode* JITCompiler::compile(ObjFunction* function) {
    if (!function || function->bytecode.empty()) return nullptr;
    if (function->bytecode.size() < 16) return nullptr;
    if (!backend_) return nullptr;

    backend_->reset();
    fixups_.clear();
    bytecode_ = function->bytecode;
    constants_ = function->constants;

    int total_bc = static_cast<int>(bytecode_.size());
    int total_inst = total_bc / 4;
    bc_to_code_.assign(total_inst + 1, -1);

    // Prologue
    backend_->emit_prologue();

    // Compile bytecode
    int pc = 0;
    while (pc < total_bc) {
        bc_to_code_[pc / 4] = static_cast<int>(backend_->code_size());
        compile_instruction(pc);
    }

    // End-of-function bailout
    emit_bailout(total_bc);

    // Fix up jumps
    fixup_jumps();

    // Finalize (allocate executable memory)
    return backend_->finalize();
}

// ============================================================
// JITCache
// ============================================================

JITCode* JITCache::get_or_compile(ObjFunction* func) {
    auto it = compiled.find(func);
    if (it != compiled.end()) return it->second;

    JITCode* code = compiler.compile(func);
    if (code) compiled[func] = code;
    return code;
}

void JITCache::record_call(ObjFunction* func) {
    call_counts[func]++;
}

JITCache::~JITCache() {
    for (auto& [func, code] : compiled) delete code;
    compiled.clear();
}

} // namespace akar
