#pragma once
#include "akar/vm/jit.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

namespace akar {

struct ObjFunction;
struct ObjClosure;
class VM;

// Trace JIT: compiles hot loops into native ARM64 code with:
// - General function inlining (any "simple" function, not just is_prime)
// - Register-allocated loop variables (callee-saved X19-X28)
// - Single type guard at entry (no per-op guards)
// - Unboxed integer arithmetic (no NaN-boxing in hot path)
//
// Patterns supported:
// 1. Filter+Accumulate: while(var <= limit) { if(f(var)) { acc += var; ... } var += step }
// 2. Pure loops: while(condition) { body }
// 3. Any hot loop with calls to "simple" (arithmetic-only) functions

class TraceCompiler {
public:
    TraceCompiler();
    ~TraceCompiler();

    // Compile a hot loop trace.
    // Returns compiled trace code, or nullptr if pattern not recognized.
    JITCode* compile(VM* vm, ObjFunction* func, int loop_start_pc, int back_edge_pc);

    // Check if a function is "inlineable" — only uses arithmetic, comparisons, branches.
    // No side effects, no nested calls, no global access.
    static bool is_inlineable(ObjFunction* func);

private:
    std::unique_ptr<JITBackend> backend_;
    VM* vm_ = nullptr;

    // Register allocation map: absolute stack slot → physical register
    struct RegMap {
        // Pre-assigned registers for hot variables
        std::unordered_map<int, int> fixed;
        // Dynamically allocated scratch registers for callee locals
        std::unordered_map<int, int> scratch;
        // Bytecode PC → code offset (for branch resolution)
        std::unordered_map<int, size_t> code_map;
        int next_scratch = 0; // X0, X1, ..., X7

        int get(int abs_slot) const {
            auto it = fixed.find(abs_slot);
            if (it != fixed.end()) return it->second;
            auto it2 = scratch.find(abs_slot);
            if (it2 != scratch.end()) return it2->second;
            return -1;
        }

        int alloc_scratch(int abs_slot) {
            // Find the next physical register NOT already in use
            std::unordered_set<int> used;
            for (auto& [s, r] : fixed) used.insert(r);
            for (auto& [s, r] : scratch) used.insert(r);
            for (int r = 0; r < 8; r++) {
                if (used.find(r) == used.end()) {
                    scratch[abs_slot] = r;
                    return r;
                }
            }
            return -1; // out of scratch registers
        }

        int get_or_alloc(int abs_slot) {
            int r = get(abs_slot);
            if (r >= 0) return r;
            return alloc_scratch(abs_slot);
        }
    };

    // Emit a single bytecode instruction as ARM64.
    // Returns true on success, false if opcode not supported.
    // Handles: LOAD_IMM, LOAD_CONST, LOAD_TRUE/FALSE, MOVE,
    //          ADD/SUB/MUL_NUM, ADD_IMM, MOD_EQ_ZERO,
    //          JMP_IF_NOT_*, JMP_IF_FALSE, JMP, RETURN
    bool emit_bc_instruction(const uint8_t* bc, int pc, int base_slot,
                             RegMap& regs, int result_phys,
                             std::vector<std::pair<size_t, int>>& fwd_fixups,
                             std::vector<size_t>& ret_fixups,
                             const std::vector<Value>* constants);

    // Emit the body of an inlineable callee.
    // Walks callee bytecode and emits ARM64 for each instruction.
    // Returns the code offset where the callee's return target is.
    bool emit_callee_body(ObjFunction* callee, int callee_base_slot,
                          int param_phys, int result_phys,
                          size_t& return_target);

    // Helpers for direct emission
    void emit_unbox(int dest_phys, int stack_slot, int base_phys);
    void emit_box_store(int src_phys, int stack_slot, int base_phys);
    void emit_guard_smi(int scratch, int stack_slot, int base_phys, size_t& bail_br);
};

} // namespace akar
