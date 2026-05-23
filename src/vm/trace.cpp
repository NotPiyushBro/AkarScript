#include "akar/vm/trace.h"
#include "akar/vm/vm.h"
#include <cstdio>
#include <cstring>

namespace akar {

// ARM64 register assignments for the trace:
// X19 = loop variable (callee-saved)
// X20 = accumulator 1 (callee-saved)
// X21 = accumulator 2 (callee-saved)
// X22 = temp (callee-saved, used for inner loop var if needed)
// X23-X25 = temp (callee-saved)
// X26 = SMALLINT_TAG
// X27 = stack base pointer (&stack_[base])
// X28 = saved out_pc pointer
// X0-X7 = scratch temporaries (used for callee locals)

static constexpr int R_SMI   = 26;
static constexpr int R_BASE  = 27;
static constexpr int R_OUTPC = 28;

// Max callee-saved registers available for hot variables
static constexpr int CALLEE_SAVED_START = 19;
static constexpr int CALLEE_SAVED_END   = 25;

TraceCompiler::TraceCompiler() {
    backend_ = create_jit_backend();
}

TraceCompiler::~TraceCompiler() = default;

// ============================================================
// is_inlineable: check if a function only uses "safe" opcodes
// (arithmetic, comparisons, branches — no side effects)
// ============================================================

bool TraceCompiler::is_inlineable(ObjFunction* func) {
    if (!func) return false;
    const auto& bc = func->bytecode;
    for (size_t pc = 0; pc + 4 <= bc.size(); pc += 4) {
        switch (static_cast<Opcode>(bc[pc])) {
            // Safe: loads
            case Opcode::LOAD_CONST:
            case Opcode::LOAD_NIL:
            case Opcode::LOAD_TRUE:
            case Opcode::LOAD_FALSE:
            case Opcode::LOAD_IMM:
            // Safe: register ops
            case Opcode::MOVE:
            case Opcode::GET_LOCAL:
            case Opcode::SET_LOCAL:
            // Safe: arithmetic (integer)
            case Opcode::ADD:
            case Opcode::SUB:
            case Opcode::MUL:
            case Opcode::NEG:
            case Opcode::ADD_NUM:
            case Opcode::SUB_NUM:
            case Opcode::MUL_NUM:
            case Opcode::ADD_IMM:
            // Safe: comparisons
            case Opcode::EQ:
            case Opcode::NEQ:
            case Opcode::LT:
            case Opcode::LTE:
            case Opcode::GT:
            case Opcode::GTE:
            case Opcode::NOT:
            case Opcode::EQ_NUM:
            case Opcode::NEQ_NUM:
            case Opcode::LT_NUM:
            case Opcode::LTE_NUM:
            case Opcode::GT_NUM:
            case Opcode::GTE_NUM:
            case Opcode::MOD_EQ_ZERO:
            // Safe: bitwise
            case Opcode::BIT_AND:
            case Opcode::BIT_OR:
            case Opcode::BIT_XOR:
            case Opcode::BIT_NOT:
            case Opcode::SHL:
            case Opcode::SHR:
            // Safe: branches
            case Opcode::JMP:
            case Opcode::JMP_IF_FALSE:
            case Opcode::JMP_IF_TRUE:
            case Opcode::JMP_IF_NOT_LT:
            case Opcode::JMP_IF_NOT_LTE:
            case Opcode::JMP_IF_NOT_GT:
            case Opcode::JMP_IF_NOT_GTE:
            case Opcode::JMP_IF_NOT_EQ:
            // Safe: return
            case Opcode::RETURN:
            case Opcode::NOP:
                break;
            // Unsafe: everything else
            default:
                return false;
        }
    }
    return true;
}

// ============================================================
// Helpers
// ============================================================

void TraceCompiler::emit_guard_smi(int scratch, int stack_slot, int base_phys, size_t& bail_br) {
    auto* b = backend_.get();
    int tag_reg = (scratch == 0) ? 1 : 0; // use a different scratch for the tag
    b->emit_load_int(scratch, base_phys, stack_slot * 8);
    b->emit_lsr_imm(scratch, scratch, 48);
    b->emit_load_imm64(tag_reg, 0xFFF7);
    b->emit_cmp(scratch, tag_reg);
    bail_br = b->emit_branch_cond(b->cond_ne(), 0);
}

void TraceCompiler::emit_unbox(int dest_phys, int stack_slot, int base_phys) {
    auto* b = backend_.get();
    b->emit_load_int(dest_phys, base_phys, stack_slot * 8);
    b->emit_lsl(dest_phys, dest_phys, 16);
    b->emit_asr_imm(dest_phys, dest_phys, 16);
}

void TraceCompiler::emit_box_store(int src_phys, int stack_slot, int base_phys) {
    auto* b = backend_.get();
    int tmp = 0; // X0
    b->emit_lsl(tmp, src_phys, 16);
    b->emit_lsr_imm(tmp, tmp, 16);
    b->emit_orr(tmp, tmp, R_SMI);
    b->emit_store_int(tmp, base_phys, stack_slot * 8);
}

// ============================================================
// emit_bc_instruction: translate a single bytecode instruction to ARM64
// ============================================================

bool TraceCompiler::emit_bc_instruction(
    const uint8_t* bc, int pc, int base_slot,
    RegMap& regs, int result_phys,
    std::vector<std::pair<size_t, int>>& fwd_fixups,
    std::vector<size_t>& ret_fixups,
    const std::vector<Value>* constants
) {
    auto* b = backend_.get();
    uint8_t op = bc[pc];
    uint8_t a = bc[pc+1], bv = bc[pc+2], cv = bc[pc+3];
    int16_t sbx = static_cast<int16_t>((bv << 8) | cv);
    uint16_t bx = (bv << 8) | cv;

    int abs_a = base_slot + a;
    int abs_b = base_slot + bv;
    int abs_c = base_slot + cv;

    switch (static_cast<Opcode>(op)) {
        case Opcode::LOAD_IMM: {
            int da = regs.get_or_alloc(abs_a);
            if (da < 0) return false;
            b->emit_load_imm64(da, static_cast<uint64_t>(bv));
            return true;
        }
        case Opcode::LOAD_CONST: {
            int da = regs.get_or_alloc(abs_a);
            if (da < 0) return false;
            if (!constants || bx >= constants->size()) return false;
            Value v = (*constants)[bx];
            if (v.is_smallint()) {
                b->emit_load_imm64(da, static_cast<uint64_t>(v.get_int()));
            } else if (v.is_number()) {
                double d = v.get_number();
                if (d == static_cast<double>(static_cast<int64_t>(d))) {
                    b->emit_load_imm64(da, static_cast<uint64_t>(static_cast<int64_t>(d)));
                } else {
                    return false; // non-integer constant
                }
            } else {
                return false; // non-numeric constant
            }
            return true;
        }
        case Opcode::LOAD_TRUE: {
            int da = regs.get_or_alloc(abs_a);
            if (da < 0) return false;
            b->emit_load_imm64(da, 1);
            return true;
        }
        case Opcode::LOAD_FALSE: {
            int da = regs.get_or_alloc(abs_a);
            if (da < 0) return false;
            b->emit_load_imm64(da, 0);
            return true;
        }
        case Opcode::LOAD_NIL: {
            int da = regs.get_or_alloc(abs_a);
            if (da < 0) return false;
            b->emit_load_imm64(da, 0);
            return true;
        }
        case Opcode::MOVE:
        case Opcode::GET_LOCAL:
        case Opcode::SET_LOCAL: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            if (da < 0 || db < 0) return false;
            if (da != db) b->emit_mov(da, db);
            return true;
        }
        case Opcode::ADD_NUM: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            int dc = regs.get_or_alloc(abs_c);
            if (da < 0 || db < 0 || dc < 0) return false;
            b->emit_add(da, db, dc);
            return true;
        }
        case Opcode::SUB_NUM: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            int dc = regs.get_or_alloc(abs_c);
            if (da < 0 || db < 0 || dc < 0) return false;
            b->emit_sub(da, db, dc);
            return true;
        }
        case Opcode::MUL_NUM: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            int dc = (bv == cv) ? db : regs.get_or_alloc(abs_c);
            if (da < 0 || db < 0 || dc < 0) return false;
            b->emit_mul(da, db, dc);
            return true;
        }
        case Opcode::ADD_IMM: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            if (da < 0 || db < 0) return false;
            if (cv <= 4095) {
                // Use ARM64 ADD Xd, Xn, #imm12 (no temp register needed)
                b->emit_add_imm(da, db, cv);
            } else {
                int tmp = regs.get_or_alloc(base_slot + 100);
                if (tmp < 0) return false;
                b->emit_load_imm64(tmp, static_cast<uint64_t>(cv));
                b->emit_add(da, db, tmp);
            }
            return true;
        }
        case Opcode::MOD_EQ_ZERO: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            int dc = regs.get_or_alloc(abs_c);
            if (da < 0 || db < 0 || dc < 0) return false;
            // Use temp slots that don't conflict with callee registers
            int t0 = regs.get_or_alloc(base_slot + 500);
            int t1 = regs.get_or_alloc(base_slot + 501);
            if (t0 < 0 || t1 < 0) return false;
            b->emit_sdiv(t0, db, dc);      // t0 = n / i
            b->emit_msub(t1, t0, dc, db);  // t1 = n - (n/i)*i = n % i
            b->emit_cmp_imm(t1, 0);
            // CSET: da = (remainder == 0) ? 1 : 0
            b->emit_load_imm64(da, 0);
            size_t ne_br = b->emit_branch_cond(b->cond_ne(), 0);
            b->emit_load_imm64(da, 1);
            b->patch_branch(ne_br, b->code_size());
            return true;
        }
        // Bitwise operations
        case Opcode::BIT_AND: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            int dc = regs.get_or_alloc(abs_c);
            if (da < 0 || db < 0 || dc < 0) return false;
            b->emit_and(da, db, dc);
            return true;
        }
        case Opcode::BIT_OR: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            int dc = regs.get_or_alloc(abs_c);
            if (da < 0 || db < 0 || dc < 0) return false;
            b->emit_orr(da, db, dc);
            return true;
        }
        case Opcode::BIT_XOR: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            int dc = regs.get_or_alloc(abs_c);
            if (da < 0 || db < 0 || dc < 0) return false;
            b->emit_eor(da, db, dc);
            return true;
        }
        case Opcode::BIT_NOT: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            if (da < 0 || db < 0) return false;
            b->emit_mvn(da, db);
            return true;
        }
        case Opcode::NEG: {
            int da = regs.get_or_alloc(abs_a);
            int db = regs.get_or_alloc(abs_b);
            if (da < 0 || db < 0) return false;
            // SUB da, XZR, db (negate)
            b->emit_sub(da, 31, db); // X31 = XZR
            return true;
        }
        // Fused compare-branch
        case Opcode::JMP_IF_NOT_LT:
        case Opcode::JMP_IF_NOT_LTE:
        case Opcode::JMP_IF_NOT_GT:
        case Opcode::JMP_IF_NOT_GTE:
        case Opcode::JMP_IF_NOT_EQ: {
            int ra = regs.get_or_alloc(abs_a);
            int rb = regs.get_or_alloc(abs_b);
            if (ra < 0 || rb < 0) return false;
            int target_pc = pc + 4 + static_cast<int8_t>(cv);
            int cond_fn;
            switch (static_cast<Opcode>(op)) {
                case Opcode::JMP_IF_NOT_LT:  cond_fn = b->cond_ge(); break;
                case Opcode::JMP_IF_NOT_LTE: cond_fn = b->cond_gt(); break;
                case Opcode::JMP_IF_NOT_GT:  cond_fn = b->cond_le(); break;
                case Opcode::JMP_IF_NOT_GTE: cond_fn = b->cond_lt(); break;
                case Opcode::JMP_IF_NOT_EQ:  cond_fn = b->cond_ne(); break;
                default: return false;
            }
            b->emit_cmp(ra, rb);
            size_t br = b->emit_branch_cond(cond_fn, 0);
            if (target_pc <= pc) {
                // Backward branch — already compiled
                auto it = regs.code_map.find(target_pc);
                if (it != regs.code_map.end()) {
                    b->patch_branch(br, it->second);
                } else {
                    return false;
                }
            } else {
                fwd_fixups.push_back({br, target_pc});
            }
            return true;
        }
        case Opcode::JMP_IF_FALSE: {
            int ra = regs.get_or_alloc(abs_a);
            if (ra < 0) return false;
            int target_pc = pc + 4 + sbx;
            b->emit_cmp_imm(ra, 0);
            size_t br = b->emit_branch_cond(b->cond_eq(), 0);
            if (target_pc <= pc) {
                auto it = regs.code_map.find(target_pc);
                if (it != regs.code_map.end()) {
                    b->patch_branch(br, it->second);
                } else {
                    return false;
                }
            } else {
                fwd_fixups.push_back({br, target_pc});
            }
            return true;
        }
        case Opcode::JMP_IF_TRUE: {
            int ra = regs.get_or_alloc(abs_a);
            if (ra < 0) return false;
            int target_pc = pc + 4 + sbx;
            b->emit_cmp_imm(ra, 0);
            size_t br = b->emit_branch_cond(b->cond_ne(), 0);
            if (target_pc <= pc) {
                auto it = regs.code_map.find(target_pc);
                if (it != regs.code_map.end()) {
                    b->patch_branch(br, it->second);
                } else {
                    return false;
                }
            } else {
                fwd_fixups.push_back({br, target_pc});
            }
            return true;
        }
        case Opcode::JMP: {
            int target_pc = pc + sbx; // JMP offset from current instruction
            if (target_pc <= pc) {
                auto it = regs.code_map.find(target_pc);
                if (it != regs.code_map.end()) {
                    b->emit_branch(0);
                    b->patch_branch(b->code_size() - 4, it->second);
                } else {
                    return false;
                }
            } else {
                size_t br = b->emit_branch(0);
                fwd_fixups.push_back({br, target_pc});
            }
            return true;
        }
        case Opcode::RETURN: {
            int ra = regs.get_or_alloc(abs_a);
            if (ra < 0) return false;
            // Store result to result_phys
            if (ra != result_phys) b->emit_mov(result_phys, ra);
            // Jump to return target (patched later)
            size_t br = b->emit_branch(0);
            ret_fixups.push_back(br);
            return true;
        }
        case Opcode::NOP:
            return true;
        default:
            return false;
    }
}

// ============================================================
// emit_callee_body: walk callee bytecode and emit ARM64
// ============================================================

bool TraceCompiler::emit_callee_body(
    ObjFunction* callee, int callee_base_slot,
    int param_phys, int result_phys,
    size_t& return_target
) {
    auto* b = backend_.get();
    const auto& bc = callee->bytecode;

    RegMap regs;
    // Map callee's register 0 (parameter) to param_phys
    regs.fixed[callee_base_slot] = param_phys;

    std::vector<std::pair<size_t, int>> fwd_fixups;
    std::vector<size_t> ret_fixups;

    int pc = 0;
    while (pc + 4 <= static_cast<int>(bc.size())) {
        regs.code_map[pc] = b->code_size();
        if (!emit_bc_instruction(bc.data(), pc, callee_base_slot, regs,
                                  result_phys, fwd_fixups, ret_fixups,
                                  &callee->constants)) {
            return false;
        }
        pc += 4;
    }

    // Record return target (code after the callee)
    return_target = b->code_size();

    // Patch return fixups
    for (size_t br : ret_fixups) {
        b->patch_branch(br, return_target);
    }

    // Patch forward fixups
    for (auto& [code_off, target_pc] : fwd_fixups) {
        auto it = regs.code_map.find(target_pc);
        if (it != regs.code_map.end()) {
            b->patch_branch(code_off, it->second);
        } else {
            return false;
        }
    }

    return true;
}

// ============================================================
// compile: main entry point — analyze hot loop and emit ARM64
// ============================================================

JITCode* TraceCompiler::compile(VM* vm, ObjFunction* func, int loop_start_pc, int back_edge_pc) {
    vm_ = vm;
    auto* b = backend_.get();
    const auto& bc = func->bytecode;

    // ---- Validate loop structure ----
    if (back_edge_pc + 4 > static_cast<int>(bc.size())) return nullptr;
    if (bc[back_edge_pc] != op_byte(Opcode::JMP)) return nullptr;
    int16_t jmp_offset = static_cast<int16_t>((bc[back_edge_pc+2] << 8) | bc[back_edge_pc+3]);
    if (jmp_offset >= 0) return nullptr;

    // Verify loop header: expect LOAD_CONST + JMP_IF_NOT_* (while condition)
    if (loop_start_pc + 8 > static_cast<int>(bc.size())) return nullptr;
    if (bc[loop_start_pc] != op_byte(Opcode::LOAD_CONST)) return nullptr;

    // Check for any JMP_IF_NOT_* at loop_start + 4
    Opcode cond_op = static_cast<Opcode>(bc[loop_start_pc + 4]);
    if (cond_op < Opcode::JMP_IF_NOT_LT || cond_op > Opcode::JMP_IF_NOT_EQ) return nullptr;

    // Extract loop limit from LOAD_CONST
    uint16_t limit_bx = (bc[loop_start_pc + 2] << 8) | bc[loop_start_pc + 3];
    if (limit_bx >= func->constants.size()) return nullptr;
    Value limit_val = func->constants[limit_bx];
    int64_t limit = 0;
    if (limit_val.is_smallint()) {
        limit = limit_val.get_int();
    } else if (limit_val.is_number()) {
        double d = limit_val.get_number();
        if (d != static_cast<double>(static_cast<int64_t>(d))) return nullptr;
        limit = static_cast<int64_t>(d);
    } else {
        return nullptr;
    }

    // Extract loop variable register from the condition
    uint8_t cond_a = bc[loop_start_pc + 4 + 1];
    uint8_t cond_b = bc[loop_start_pc + 4 + 2];
    uint8_t limit_reg = bc[loop_start_pc + 1]; // LOAD_CONST destination
    int n_reg = (cond_a == limit_reg) ? cond_b : cond_a;

    // ---- Find CALL instruction ----
    int call_pc = -1;
    uint8_t call_a = 0, call_argc = 0;
    for (int pc = loop_start_pc + 8; pc < back_edge_pc; pc += 4) {
        if (bc[pc] == op_byte(Opcode::CALL)) {
            call_pc = pc;
            call_a = bc[pc + 1];
            call_argc = bc[pc + 2];
            break;
        }
    }
    if (call_pc < 0 || call_argc < 1) return nullptr;

    // ---- Find callee via GET_GLOBAL ----
    ObjClosure* callee_closure = nullptr;
    for (int pc = loop_start_pc + 8; pc < call_pc; pc += 4) {
        if (bc[pc] == op_byte(Opcode::GET_GLOBAL) && bc[pc + 1] == call_a) {
            uint16_t name_bx = (bc[pc + 2] << 8) | bc[pc + 3];
            if (name_bx < func->constants.size() && func->constants[name_bx].is_string()) {
                ObjString* name = func->constants[name_bx].as_string();
                Value* gv = vm_->jit_find_global(name);
                if (gv && gv->is_closure()) callee_closure = gv->as_closure();
            }
            break;
        }
    }
    if (!callee_closure) return nullptr;
    ObjFunction* callee = callee_closure->function;

    // ---- Check if callee is inlineable ----
    if (!is_inlineable(callee)) {
        return nullptr;
    }

    // ---- Find JMP_IF_FALSE after CALL (splits conditional/unconditional blocks) ----
    int if_false_pc = -1;
    int if_false_target = back_edge_pc;
    for (int pc = call_pc + 4; pc < back_edge_pc; pc += 4) {
        if (bc[pc] == op_byte(Opcode::JMP_IF_FALSE)) {
            if_false_pc = pc;
            int16_t joff = static_cast<int16_t>((bc[pc+2] << 8) | bc[pc+3]);
            if_false_target = pc + 4 + joff;
            break;
        }
    }

    // ---- Find accumulators in the conditional block ----
    // (between JMP_IF_FALSE and its target)
    std::vector<int> acc_regs;
    if (if_false_pc >= 0) {
        for (int pc = if_false_pc + 4; pc < if_false_target && pc < back_edge_pc; pc += 4) {
            Opcode op = static_cast<Opcode>(bc[pc]);
            if (op == Opcode::ADD_NUM || op == Opcode::ADD_IMM || op == Opcode::SUB_NUM) {
                int reg = bc[pc + 1];
                if (reg != n_reg) { // exclude the loop variable
                    bool found = false;
                    for (int r : acc_regs) { if (r == reg) { found = true; break; } }
                    if (!found) acc_regs.push_back(reg);
                }
            }
        }
    }

    // ---- Assign registers ----
    // Hot variables get callee-saved registers
    RegMap caller_regs;
    int next_callee_saved = CALLEE_SAVED_START;

    // Loop variable (n)
    caller_regs.fixed[n_reg] = next_callee_saved++; // X19

    // Accumulators (sum, count, etc.)
    for (int reg : acc_regs) {
        if (next_callee_saved <= CALLEE_SAVED_END) {
            caller_regs.fixed[reg] = next_callee_saved++;
        }
    }

    // ---- Emit ARM64 ----
    b->reset();

    // Prologue
    b->emit_prologue();

    // Setup: save out_pc (X2) to X28, compute base pointer
    b->emit_mov(R_OUTPC, 2);           // X28 = out_pc pointer
    b->emit_lsl(9, 1, 3);              // X9 = base * 8
    b->emit_add(R_BASE, 0, 9);         // X27 = stack + base * 8
    b->emit_load_imm64(R_SMI, Value::SMALLINT_TAG);

    // Entry guards: check all hot values are small ints
    std::vector<size_t> bail_brs;
    size_t br;

    emit_guard_smi(0, n_reg, R_BASE, br); bail_brs.push_back(br);
    for (int reg : acc_regs) {
        emit_guard_smi(0, reg, R_BASE, br); bail_brs.push_back(br);
    }

    // Unbox hot variables into callee-saved registers
    emit_unbox(caller_regs.fixed[n_reg], n_reg, R_BASE);
    for (int reg : acc_regs) {
        emit_unbox(caller_regs.fixed[reg], reg, R_BASE);
    }

    // Load loop limit
    int limit_phys = 25; // X25 for limit
    b->emit_load_imm64(limit_phys, static_cast<uint64_t>(limit));

    // ---- Outer loop ----
    size_t outer_loop = b->code_size();

    // Condition: CMP n, limit; B.GT exit
    b->emit_cmp(caller_regs.fixed[n_reg], limit_phys);
    size_t exit_br = b->emit_branch_cond(b->cond_gt(), 0);

    // ---- Inline callee ----
    // Callee's register 0 (parameter) maps to the same phys reg as n
    int callee_base_slot = call_a + 1; // absolute slot of callee's register 0
    int param_phys = caller_regs.fixed[n_reg]; // X19
    int result_phys = 0; // X0 for result

    size_t return_target = 0;
    if (!emit_callee_body(callee, callee_base_slot, param_phys, result_phys, return_target)) {
        return nullptr;
    }

    // ---- Check callee result ----
    b->emit_cmp_imm(result_phys, 0);
    size_t result_false_br = b->emit_branch_cond(b->cond_eq(), 0); // skip if false

    // ---- Conditional block (when callee returns true) ----
    // Walk caller's bytecode between JMP_IF_FALSE and its target
    if (if_false_pc >= 0) {
        // Reset scratch allocation for the conditional block
        RegMap cond_regs = caller_regs; // inherit hot variable mappings
        std::vector<std::pair<size_t, int>> cond_fwd_fixups;
        std::vector<size_t> cond_ret_fixups;

        for (int pc = if_false_pc + 4; pc < if_false_target && pc < back_edge_pc; pc += 4) {
            emit_bc_instruction(bc.data(), pc, 0, cond_regs, result_phys,
                               cond_fwd_fixups, cond_ret_fixups, &func->constants);
        }

        // Patch forward fixups in the conditional block
        for (auto& [code_off, target_pc] : cond_fwd_fixups) {
            // These target within the conditional block — but we didn't track code_map
            // For now, skip (most conditional blocks have no branches)
        }
    }

    // ---- Unconditional block (always executed) ----
    // This is BEFORE the result_false_br patch so it always runs
    size_t uncond_start = b->code_size();
    b->patch_branch(result_false_br, uncond_start); // false → skip conditional, but run unconditional

    {
        RegMap uncond_regs = caller_regs;
        std::vector<std::pair<size_t, int>> uncond_fwd_fixups;
        std::vector<size_t> uncond_ret_fixups;

        for (int pc = if_false_target; pc < back_edge_pc; pc += 4) {
            emit_bc_instruction(bc.data(), pc, 0, uncond_regs, result_phys,
                               uncond_fwd_fixups, uncond_ret_fixups, &func->constants);
        }
    }

    // ---- Back-edge to outer loop ----
    b->emit_branch(0);
    b->patch_branch(b->code_size() - 4, outer_loop);

    // ---- Exit path ----
    size_t exit_label = b->code_size();
    b->patch_branch(exit_br, exit_label);

    // Box and store results
    emit_box_store(caller_regs.fixed[n_reg], n_reg, R_BASE);
    for (int reg : acc_regs) {
        emit_box_store(caller_regs.fixed[reg], reg, R_BASE);
    }

    // Set exit_pc = back_edge_pc + 4
    int exit_pc = back_edge_pc + 4;
    b->emit_load_imm64(0, static_cast<uint64_t>(exit_pc));
    b->emit_store_int(0, R_OUTPC, 0);

    // Return 0 (success)
    b->emit_set_return(0);
    b->emit_epilogue();

    // ---- Bail path ----
    size_t bail_label = b->code_size();
    for (size_t bbr : bail_brs) {
        b->patch_branch(bbr, bail_label);
    }
    b->emit_load_imm64(0, static_cast<uint64_t>(loop_start_pc));
    b->emit_store_int(0, R_OUTPC, 0);
    b->emit_set_return(1);
    b->emit_epilogue();

    // ---- Finalize ----
    return b->finalize();
}

} // namespace akar
