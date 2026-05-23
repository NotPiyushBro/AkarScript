#pragma once
#include "akar/vm/jit.h"
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace akar {

struct ObjFunction;
struct ObjClosure;
class VM;

// Trace JIT: compiles hot loops into native ARM64 code with:
// - Inlined function calls
// - Register-allocated loop variables (callee-saved X19-X28)
// - Single type guard at entry (no per-op guards)
// - Unboxed integer arithmetic (no NaN-boxing in hot path)

class TraceCompiler {
public:
    TraceCompiler();
    ~TraceCompiler();

    // Compile a hot loop trace.
    // Returns compiled trace code, or nullptr if pattern not recognized.
    JITCode* compile(VM* vm, ObjFunction* func, int loop_start_pc, int back_edge_pc);

private:
    std::unique_ptr<JITBackend> backend_;
    VM* vm_ = nullptr;

    // Emit a direct ARM64 loop for a recognized pattern.
    // Returns true if pattern was recognized and compiled.
    bool compile_direct_loop(ObjFunction* func, int loop_start_pc, int back_edge_pc);

    // Helpers for direct emission
    void emit_unbox(int dest_phys, int stack_slot, int base_phys);
    void emit_box_store(int src_phys, int stack_slot, int base_phys);
    void emit_guard_smi(int scratch, int stack_slot, int base_phys, size_t& bail_br);
    void emit_bail_path(size_t bail_offset);
};

} // namespace akar
