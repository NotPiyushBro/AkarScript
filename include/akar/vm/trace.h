#pragma once
// Trace JIT compiler for Akar Script
// Phase 1: Hot loop detection + trace recording + ARM64 codegen
// Target: beat LuaJIT on sum_primes (10ms → ~8ms)

#include "akar/common/value.h"
#include "akar/common/opcodes.h"
#include "akar/vm/jit.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>

namespace akar {

class VM;
struct ObjFunction;
struct ObjClosure;

// ============================================================
// Trace instruction — lightweight IR for trace compilation
// ============================================================

enum class TOp : uint8_t {
    NOP,
    // Integer arithmetic (unboxed 64-bit ints)
    IADD, ISUB, IMUL, ISDIV, IMSUB,
    // Bitwise
    IAND, IORR, ITST,
    // Comparison (sets flags)
    ICMP, ICMP_IMM,
    // Move
    MOV, MOV_IMM,
    // Memory bridge (VM stack <-> trace regs)
    LOAD_SLOT,      // rd = unbox(stack[slot])
    STORE_SLOT,     // stack[slot] = box(rs1)
    // Control flow
    LABEL, B, B_COND, CBZ, CBNZ,
    // Type guard at trace entry
    GUARD_SMI,
    // Trace exits
    EXIT_NORMAL,
    EXIT_BAIL,
    WRITEBACK,
    // Inlining markers
    INLINE_ENTER, INLINE_EXIT,
};

enum class TCond : uint8_t {
    EQ = 0, NE = 1,
    LT = 11, GE = 10, GT = 12, LE = 13,
};

using TReg = int;
constexpr TReg TREG_NONE = -1;

struct TInsn {
    TOp op = TOp::NOP;
    TReg rd = TREG_NONE;
    TReg rs1 = TREG_NONE;
    TReg rs2 = TREG_NONE;
    TReg rs3 = TREG_NONE;
    int64_t imm = 0;
    int slot = -1;
    int label_id = -1;
    TCond cond = TCond::EQ;
    int bc_pc = -1;
};

struct Trace {
    std::vector<TInsn> insns;
    int loop_header = -1;
    int loop_back = -1;

    struct RegInfo {
        int phys_reg = -1;
        int vm_slot = -1;
        bool is_callee_saved = false;
        bool is_live_in_loop = false;
    };
    std::vector<RegInfo> regs;
    int next_treg = 0;

    int alloc_treg() {
        regs.push_back({});
        return next_treg++;
    }
    int map_slot(int slot) {
        for (int i = 0; i < next_treg; i++)
            if (regs[i].vm_slot == slot) return i;
        int tr = alloc_treg();
        regs[tr].vm_slot = slot;
        return tr;
    }
};

// ============================================================
// TraceBuilder — record a trace from bytecode
// ============================================================

class TraceBuilder {
public:
    TraceBuilder(VM* vm) : vm_(vm) {}
    Trace* build(ObjFunction* func, int loop_start, int loop_end);

private:
    VM* vm_;
    Trace* trace_ = nullptr;
    struct Frame { ObjFunction* func; int base_slot; };
    std::vector<Frame> frames_;

    void emit(TInsn insn);
    bool walk_bytecode(int start, int end);
    bool handle_instruction(int& pc);
    bool inline_call(ObjClosure* closure, int arg_slot, int ret_slot, int argc);
    int treg_for_slot(int slot) { return trace_->map_slot(slot); }
};

// ============================================================
// TraceCompiler — compile trace to ARM64
// ============================================================

class TraceCompiler {
public:
    typedef int (*trace_fn_t)(Value* stack, int base, Value* constants,
                               int* out_pc, ObjClosure* closure);

    TraceCompiler();
    ~TraceCompiler();
    JITCode* compile(Trace* trace);

private:
    std::unique_ptr<JITBackend> backend_;

    struct RegAlloc {
        static constexpr int CALLEE[8] = {19,20,21,22,23,24,27,28};
        std::unordered_map<int,int> t2p, p2t;
        int next_callee = 0, next_scratch = 0;
        int alloc_callee(int treg);
        int alloc_scratch(int treg);
        int get(int treg) const;
    } ralloc_;

    std::unordered_map<int, size_t> label_defs;
    std::vector<std::pair<size_t, int>> label_refs;

    void emit_prologue(Trace* trace);
    void emit_epilogue(Trace* trace);
    void emit_guard(Trace* trace);
    void emit_writeback(Trace* trace);
    void emit_insn(const TInsn& insn);
    void patch_labels();
};

// ============================================================
// TraceCache — detect hot loops and cache compiled traces
// ============================================================

struct TraceCache {
    std::unordered_map<int, JITCode*> traces;
    std::unordered_map<int, int> counts;
    static constexpr int THRESHOLD = 100;

    TraceCache(VM* vm);
    ~TraceCache();
    JITCode* check_hot(int backedge_pc, ObjFunction* func);

private:
    VM* vm_;
};

} // namespace akar
