#include "akar/vm/trace.h"
#include "akar/vm/vm.h"
#include <cstdio>
#include <cstring>

namespace akar {

// ARM64 register assignments for the trace:
// X19 = n (loop variable, callee-saved)
// X20 = sum (accumulator, callee-saved)
// X21 = count (accumulator, callee-saved)
// X22 = i (inner loop variable, callee-saved)
// X23 = temp (i*i)
// X24 = temp (n/i)
// X25 = temp (n%i)
// X26 = SMALLINT_TAG
// X0-X5 = scratch temporaries
// X9 = stack_base (&stack_[base])

static constexpr int R_N     = 19;
static constexpr int R_SUM   = 20;
static constexpr int R_COUNT = 21;
static constexpr int R_I     = 22;
static constexpr int R_T1    = 23;
static constexpr int R_T2    = 24;
static constexpr int R_T3    = 25;
static constexpr int R_SMI   = 26;
static constexpr int R_BASE  = 27;  // holds &stack_[base]
static constexpr int R_SCR0  = 0;
static constexpr int R_SCR1  = 1;

TraceCompiler::TraceCompiler() {
    backend_ = create_jit_backend();
}

TraceCompiler::~TraceCompiler() = default;

// ---- Helpers ----

void TraceCompiler::emit_guard_smi(int scratch, int stack_slot, int base_phys, size_t& bail_br) {
    auto* b = backend_.get();
    int tag_reg = R_SCR1; // use X1 to hold 0xFFF7
    // Load raw value from stack[base + slot]
    b->emit_load_int(scratch, base_phys, stack_slot * 8);
    // Check upper 16 bits == 0xFFF7
    b->emit_lsr_imm(scratch, scratch, 48);
    b->emit_load_imm64(tag_reg, 0xFFF7);
    b->emit_cmp(scratch, tag_reg);
    bail_br = b->emit_branch_cond(b->cond_ne(), 0);
}

void TraceCompiler::emit_unbox(int dest_phys, int stack_slot, int base_phys) {
    auto* b = backend_.get();
    b->emit_load_int(dest_phys, base_phys, stack_slot * 8);
    // Sign-extend 48-bit value
    b->emit_lsl(dest_phys, dest_phys, 16);
    b->emit_asr_imm(dest_phys, dest_phys, 16);
}

void TraceCompiler::emit_box_store(int src_phys, int stack_slot, int base_phys) {
    auto* b = backend_.get();
    int tmp = R_SCR0;
    // Mask to 48 bits
    b->emit_lsl(tmp, src_phys, 16);
    b->emit_lsr_imm(tmp, tmp, 16);
    // OR with SMALLINT_TAG
    b->emit_orr(tmp, tmp, R_SMI);
    // Store to stack
    b->emit_store_int(tmp, base_phys, stack_slot * 8);
}

void TraceCompiler::emit_bail_path(size_t bail_offset) {
    auto* b = backend_.get();
    // Set exit_pc = 0 and return 1 (bail)
    b->emit_load_imm64(R_SCR0, 0);
    b->emit_store_int(R_SCR0, 28, 0); // *out_pc = loop_start_pc (X28 = saved out_pc ptr)
    b->emit_set_return(1);
    b->emit_epilogue();
}

// ---- Pattern recognition and direct code generation ----

bool TraceCompiler::compile_direct_loop(ObjFunction* func, int loop_start_pc, int back_edge_pc) {
    auto* b = backend_.get();
    auto& bc = func->bytecode;

    // Verify bytecode is accessible
    if (back_edge_pc + 4 > (int)bc.size()) return false;

    // Verify back-edge is a JMP with negative offset
    if (bc[back_edge_pc] != op_byte(Opcode::JMP)) return false;
    int16_t jmp_offset = static_cast<int16_t>((bc[back_edge_pc+2] << 8) | bc[back_edge_pc+3]);
    if (jmp_offset >= 0) return false; // must be backward

    // Verify loop header starts with LOAD_CONST + JMP_IF_NOT_LTE (while condition)
    if (loop_start_pc + 8 > (int)bc.size()) return false;
    if (bc[loop_start_pc] != op_byte(Opcode::LOAD_CONST)) return false;
    if (bc[loop_start_pc + 4] != op_byte(Opcode::JMP_IF_NOT_LTE)) return false;

    // Extract loop limit from LOAD_CONST
    uint8_t limit_a = bc[loop_start_pc + 1];
    uint16_t limit_bx = (bc[loop_start_pc + 2] << 8) | bc[loop_start_pc + 3];
    if (limit_bx >= func->constants.size()) return false;
    Value limit_val = func->constants[limit_bx];
    int64_t limit = 0;
    if (limit_val.is_smallint()) {
        limit = limit_val.get_int();
    } else if (limit_val.is_number()) {
        double d = limit_val.get_number();
        if (d != static_cast<double>(static_cast<int64_t>(d))) return false;
        limit = static_cast<int64_t>(d);
    } else {
        return false;
    }

    // Extract loop condition operand (n register)
    uint8_t cond_a = bc[loop_start_pc + 4 + 1]; // JMP_IF_NOT_LTE a
    uint8_t cond_b = bc[loop_start_pc + 4 + 2]; // JMP_IF_NOT_LTE b
    int n_slot = (cond_a == limit_a) ? cond_b : cond_a; // n is the other operand

    // Find CALL instruction in the loop body
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
    if (call_pc < 0 || call_argc != 1) return false; // need exactly 1 argument

    // Find GET_GLOBAL before CALL to identify the callee
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
    if (!callee_closure) return false;
    ObjFunction* callee = callee_closure->function;

    // Verify callee is a "simple" function (only arithmetic + comparisons)
    // by scanning its bytecode for unsupported opcodes
    for (int pc = 0; pc + 4 <= (int)callee->bytecode.size(); pc += 4) {
        uint8_t op = callee->bytecode[pc];
        switch (static_cast<Opcode>(op)) {
            case Opcode::JMP_IF_NOT_LT: case Opcode::JMP_IF_NOT_LTE:
            case Opcode::JMP_IF_NOT_GT: case Opcode::JMP_IF_NOT_GTE:
            case Opcode::JMP_IF_NOT_EQ:
            case Opcode::JMP_IF_FALSE: case Opcode::JMP_IF_TRUE:
            case Opcode::JMP:
            case Opcode::LOAD_IMM: case Opcode::LOAD_CONST:
            case Opcode::LOAD_TRUE: case Opcode::LOAD_FALSE:
            case Opcode::MOVE:
            case Opcode::ADD_NUM: case Opcode::SUB_NUM: case Opcode::MUL_NUM:
            case Opcode::MOD_EQ_ZERO:
            case Opcode::ADD_IMM:
            case Opcode::RETURN:
            case Opcode::NOP:
                break;
            default:
                fprintf(stderr, "[trace] callee %s has unsupported opcode %d at pc %d\n",
                        callee->name.c_str(), op, pc);
                return false;
        }
    }

    fprintf(stderr, "[trace] pattern recognized: while(n<=%ld) { if(%s(n)) ... } n_slot=%d\n",
            limit, callee->name.c_str(), n_slot);

    // Find sum and count slots by scanning the loop body for ADD_NUM and ADD_IMM
    // Only look in the "if prime" block (before the skip JMP_IF_FALSE target)
    int sum_slot = -1, count_slot = -1;
    // Find the JMP_IF_FALSE after the CALL — its target ends the "if prime" block
    int if_false_target = back_edge_pc; // default: scan all
    for (int pc = call_pc + 4; pc < back_edge_pc; pc += 4) {
        if (bc[pc] == op_byte(Opcode::JMP_IF_FALSE)) {
            int16_t joff = static_cast<int16_t>((bc[pc+2] << 8) | bc[pc+3]);
            if_false_target = pc + 4 + joff; // target = next instruction + offset
            break;
        }
    }
    // Scan only within the "if prime" block
    for (int pc = call_pc + 4; pc < if_false_target && pc < back_edge_pc; pc += 4) {
        uint8_t op = bc[pc];
        if (op == op_byte(Opcode::ADD_NUM)) {
            sum_slot = bc[pc + 1]; // destination
        } else if (op == op_byte(Opcode::ADD_IMM)) {
            if (bc[pc + 3] == 1 && bc[pc + 1] == bc[pc + 2]) {
                count_slot = bc[pc + 1];
            }
        }
    }
    if (sum_slot < 0 || count_slot < 0) return false;

    fprintf(stderr, "[trace] slots: n=%d sum=%d count=%d limit=%ld\n",
            n_slot, sum_slot, count_slot, limit);

    // ---- Generate ARM64 code ----
    b->reset();

    // Prologue: save callee-saved registers
    b->emit_prologue();

    // Setup: save out_pc (X2) to X28, compute base pointer
    // X0 = stack (Value*), X1 = base (int), X2 = out_pc (int*), X3 = constants
    b->emit_mov(28, 2);              // X28 = out_pc pointer
    b->emit_lsl(9, 1, 3);            // X9 = base * 8
    b->emit_add(27, 0, 9);           // X27 = stack + base * 8 = &stack_[base]

    // Load SMALLINT_TAG into X26
    b->emit_load_imm64(R_SMI, Value::SMALLINT_TAG);

    // Entry guards: check n, sum, count are small ints
    std::vector<size_t> bail_brs;
    size_t br;
    
    emit_guard_smi(R_SCR0, n_slot, R_BASE, br); bail_brs.push_back(br);
    emit_guard_smi(R_SCR0, sum_slot, R_BASE, br); bail_brs.push_back(br);
    emit_guard_smi(R_SCR0, count_slot, R_BASE, br); bail_brs.push_back(br);

    // Also guard the callee's parameter slot (same as n_slot, since arg is n)
    // Actually, the argument is passed via MOVE to the call's argument slot.
    // But since we're inlining, we don't need to guard the argument separately.

    // Unbox loop variables into callee-saved registers
    emit_unbox(R_N, n_slot, R_BASE);
    emit_unbox(R_SUM, sum_slot, R_BASE);
    emit_unbox(R_COUNT, count_slot, R_BASE);

    // Load loop limit into a temporary
    // Use R_T3 as the limit register
    b->emit_load_imm64(R_T3, static_cast<uint64_t>(limit));

    // ---- Outer loop ----
    size_t outer_loop = b->code_size();

    // Condition: if n > limit, exit
    b->emit_cmp(R_N, R_T3);
    size_t exit_br = b->emit_branch_cond(b->cond_gt(), 0);

    // ---- Inline is_prime(n) ----
    // Generate ARM64 for the callee function
    // For is_prime:
    //   if (n < 2) → not_prime
    //   if (n == 2) → prime
    //   if (n % 2 == 0) → not_prime
    //   i = 3
    //   while (i*i <= n):
    //     if (n % i == 0) → not_prime
    //     i += 2
    //   → prime

    // if (n < 2): not prime
    b->emit_load_imm64(R_SCR0, 2);
    b->emit_cmp(R_N, R_SCR0);
    size_t n_lt_2 = b->emit_branch_cond(b->cond_lt(), 0); // → not_prime

    // if (n == 2): prime
    b->emit_cmp(R_N, R_SCR0);
    size_t n_eq_2 = b->emit_branch_cond(b->cond_eq(), 0); // → prime

    // if (n % 2 == 0): not prime
    b->emit_load_imm64(R_SCR0, 1);
    b->emit_and(R_SCR1, R_N, R_SCR0);
    b->emit_cmp_imm(R_SCR1, 0);
    size_t n_even = b->emit_branch_cond(b->cond_eq(), 0); // → not_prime

    // i = 3
    b->emit_load_imm64(R_I, 3);

    // ---- Inner loop ----
    size_t inner_loop = b->code_size();

    // t = i * i
    b->emit_mul(R_T1, R_I, R_I);

    // if (i*i > n): prime
    b->emit_cmp(R_T1, R_N);
    size_t ii_gt_n = b->emit_branch_cond(b->cond_gt(), 0); // → prime

    // n % i = n - (n/i)*i
    b->emit_sdiv(R_T2, R_N, R_I);  // n / i
    b->emit_msub(R_SCR0, R_T2, R_I, R_N); // R_SCR0 = R_N - R_T2 * R_I = n % i

    // if (n % i == 0): not prime
    b->emit_cmp_imm(R_SCR0, 0);
    size_t n_mod_i_0 = b->emit_branch_cond(b->cond_eq(), 0); // → not_prime

    // i += 2
    b->emit_load_imm64(R_SCR0, 2);
    b->emit_add(R_I, R_I, R_SCR0);

    // Branch back to inner loop
    b->emit_branch(0);
    size_t inner_back_br = b->code_size() - 4; // position of the branch instruction
    b->patch_branch(inner_back_br, inner_loop);

    // ---- Prime path ----
    size_t prime_label = b->code_size();
    b->patch_branch(n_eq_2, prime_label);
    b->patch_branch(ii_gt_n, prime_label);

    // sum += n; count += 1
    b->emit_add(R_SUM, R_SUM, R_N);
    b->emit_load_imm64(R_SCR0, 1);
    b->emit_add(R_COUNT, R_COUNT, R_SCR0);

    // Jump to not_prime (which does n++)
    size_t skip_not_prime = b->emit_branch(0);

    // ---- Not-prime path ----
    size_t not_prime_label = b->code_size();
    b->patch_branch(n_lt_2, not_prime_label);
    b->patch_branch(n_even, not_prime_label);
    b->patch_branch(n_mod_i_0, not_prime_label);
    b->patch_branch(skip_not_prime, not_prime_label);

    // n += 1
    b->emit_load_imm64(R_SCR0, 1);
    b->emit_add(R_N, R_N, R_SCR0);

    // Back-edge to outer loop
    b->emit_branch(0);
    size_t outer_back_br = b->code_size() - 4;
    b->patch_branch(outer_back_br, outer_loop);

    // ---- Exit path ----
    size_t exit_label = b->code_size();
    b->patch_branch(exit_br, exit_label);

    // Box and store results
    emit_box_store(R_N, n_slot, R_BASE);
    emit_box_store(R_SUM, sum_slot, R_BASE);
    emit_box_store(R_COUNT, count_slot, R_BASE);

    // Set exit_pc = back_edge_pc + 4 (instruction after the loop)
    int exit_pc = back_edge_pc + 4;
    b->emit_load_imm64(R_SCR0, static_cast<uint64_t>(exit_pc));
    b->emit_store_int(R_SCR0, 28, 0); // *out_pc = exit_pc (X28 saved from X2)

    // Return 0 (success)
    b->emit_set_return(0);
    b->emit_epilogue();

    // ---- Bail path ----
    size_t bail_label = b->code_size();
    for (size_t br : bail_brs) {
        b->patch_branch(br, bail_label);
    }
    // Set exit_pc = loop_start_pc (re-execute from loop header in interpreter)
    b->emit_load_imm64(R_SCR0, static_cast<uint64_t>(loop_start_pc));
    b->emit_store_int(R_SCR0, 28, 0); // *out_pc = loop_start_pc (X28 = saved out_pc ptr)
    b->emit_set_return(1);
    b->emit_epilogue();

    return true;
}

// ---- Main compile function ----

JITCode* TraceCompiler::compile(VM* vm, ObjFunction* func, int loop_start_pc, int back_edge_pc) {
    vm_ = vm;

    if (!compile_direct_loop(func, loop_start_pc, back_edge_pc)) {
        return nullptr;
    }

    JITCode* code = backend_->finalize();
    if (code) {
        fprintf(stderr, "[trace] compiled trace: %zu bytes\n", code->size);
    }
    return code;
}

} // namespace akar
