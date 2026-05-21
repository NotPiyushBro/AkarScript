#pragma once
#include <cstdint>

namespace akar {

// Register-based bytecode opcodes
// Normal format: [opcode:8] [A:8] [B:8] [C:8] = 4 bytes
// Wide format:   [WIDE:8] [opcode:8] [A:16] [B:16] [C:16] = 8 bytes
// For some ops, B and C are combined as a 16-bit operand (BX)
enum class Opcode : uint8_t {
    // Load/store
    LOAD_CONST,     // A = constants[BX] (B<<8|C)
    LOAD_NIL,       // A = nil
    LOAD_TRUE,      // A = true
    LOAD_FALSE,     // A = false

    // Register ops
    MOVE,           // A = R[B]
    // Stack ops (for function calls)
    GET_LOCAL,      // A = stack[BP + B]
    SET_LOCAL,      // stack[BP + B] = A
    GET_UPVALUE,    // A = upvalue[B]
    SET_UPVALUE,    // upvalue[B] = A
    GET_GLOBAL,     // A = globals[name_at_B]
    SET_GLOBAL,     // globals[name_at_B] = A

    // Arithmetic
    ADD,            // A = R[B] + R[C]
    SUB,            // A = R[B] - R[C]
    MUL,            // A = R[B] * R[C]
    DIV,            // A = R[B] / R[C]
    MOD,            // A = R[B] % R[C]
    NEG,            // A = -R[B]

    // Comparison
    EQ,             // A = R[B] == R[C]
    NEQ,            // A = R[B] != R[C]
    LT,             // A = R[B] < R[C]
    LTE,            // A = R[B] <= R[C]
    GT,             // A = R[B] > R[C]
    GTE,            // A = R[B] >= R[C]

    // Logical
    NOT,            // A = !R[B]

    // Control flow
    JMP,            // PC += signed_offset in BX (B<<8|C, interpreted as signed)
    JMP_IF_FALSE,   // if (!R[A]) PC += signed_offset in BX
    JMP_IF_TRUE,    // if (R[A]) PC += signed_offset in BX

    // Functions
    CALL,           // R[A] = R[A](R[A+1], ..., R[A+B])  B = arg_count
    CLOSURE,        // A = closure(constants[BX]) with upvalues in following slots
    CLOSE_UPVALUE,  // close upvalues at stack depth A
    RETURN,         // return R[A]

    // Data structures
    NEW_ARRAY,      // A = new array; elements from R[A+1]..R[A+B]
    NEW_MAP,        // A = new map; key-value pairs from stack
    GET_INDEX,      // A = R[B][R[C]]
    SET_INDEX,      // R[A][R[B]] = R[C]
    GET_FIELD,      // A = R[B].field_name_constant_C
    SET_FIELD,      // R[A].field_name_constant_B = R[C]

    // Class/object
    NEW_CLASS,      // A = new class(name_constant_BX)
    NEW_INSTANCE,   // A = new instance of R[B]
    GET_METHOD,     // A = R[B].method_name_C
    INVOKE,         // A = R[B].method_name_C(args: R[B+1]..R[B+D]), D in high bits

    // Range
    NEW_RANGE,      // A = range(R[B], R[C])

    // Iterator (for "for x in collection")
    ITER_INIT,      // A = iterator(R[B])
    ITER_NEXT,      // A = iter_next(R[B]); sets done flag
    ITER_DONE,      // A = iter_done(R[B])

    // Special
    PRINT,          // print R[A]
    HALT,           // stop execution
    NOP,            // no operation

    // Quickened (type-specialized) opcodes - emitted by VM at runtime
    // These skip type checks since types were verified on first execution
    ADD_NUM,        // A = R[B] + R[C] (both known numbers)
    SUB_NUM,        // A = R[B] - R[C] (both known numbers)
    MUL_NUM,        // A = R[B] * R[C] (both known numbers)
    DIV_NUM,        // A = R[B] / R[C] (both known numbers)
    MOD_NUM,        // A = R[B] % R[C] (both known numbers)
    ADD_STR,        // A = R[B] .. R[C] (both known strings)
    EQ_NUM,         // A = R[B] == R[C] (both known numbers)
    NEQ_NUM,        // A = R[B] != R[C] (both known numbers)
    LT_NUM,         // A = R[B] < R[C] (both known numbers)
    LTE_NUM,        // A = R[B] <= R[C] (both known numbers)
    GT_NUM,         // A = R[B] > R[C] (both known numbers)
    GTE_NUM,        // A = R[B] >= R[C] (both known numbers)

    // Compiler-emitted fused opcodes (reduce dispatch in hot loops)
    MOD_EQ_ZERO,    // A = (R[B] % R[C] == 0) — fuses MOD + EQ + zero check into 1 opcode

    // Small integer inline encoding
    LOAD_IMM,       // A = B (immediate 8-bit value, 0-255) — no constant table lookup
    ADD_IMM,        // A = R[B] + C (immediate 8-bit add) — fuses LOAD_IMM + ADD

    // Fiber/Coroutine
    FIBER_YIELD,    // yield R[A] from current fiber, resume value goes to R[A]
    FIBER_RESUME,   // resume fiber R[B], pass R[C] as resume value, result in R[A]

    // Tail call optimization
    TAIL_CALL,      // R[A] = R[A](R[A+1], ..., R[A+B]) reusing current frame. B = arg_count

    // Await
    AWAIT,          // if R[A] is nil, return (suspend fiber); otherwise continue

    // Exception handling
    THROW,          // throw R[A] as exception
    TRY_BEGIN,      // mark start of try block, BX = catch offset
    TRY_END,        // mark end of try block

    // Wide prefix - next instruction uses 16-bit register fields
    WIDE,           // prefix: next instruction is [op:8][A:16][B:16][C:16] = 7 bytes

    // Signal & Effect (Reactive primitives)
    SIGNAL_CREATE,  // A = signal(B) — create signal with initial value from R[B]
    SIGNAL_GET,     // A = signal_read(B) — read signal value, track dependency if in effect
    SIGNAL_SET,     // signal_write(A, B) — update signal R[A] with value R[B], notify effects

    EFFECT_CREATE,  // A = effect(B) — create effect from closure R[B]
    EFFECT_RUN,     // run_effect(A) — schedule effect R[A] for immediate execution

    // Enum
    ENUM_CREATE,    // A = enum(name_BX) — create enum class with name from constants[BX]
    ENUM_VARIANT,   // set_variant(A, B, C) — register variant: class R[A], name const_B, simple value R[C]
    ENUM_DATA_VARIANT, // set_data_variant(A, B) — register data variant: class R[A], name const_B (creates factory method)
    ENUM_GET,       // A = R[B].variant_C — get enum variant (constant index C)
    ENUM_IS,        // A = is_enum_type(R[B], name_const_C) — check if R[B] belongs to enum with name const_C
};

inline uint8_t op_byte(Opcode op) { return static_cast<uint8_t>(op); }

// Encode instruction
inline uint32_t make_instruction(Opcode op, uint8_t a, uint8_t b, uint8_t c) {
    return (op_byte(op) << 24) | (a << 16) | (b << 8) | c;
}

inline uint32_t make_instruction_bx(Opcode op, uint8_t a, uint16_t bx) {
    return (op_byte(op) << 24) | (a << 16) | bx;
}

// Decode
inline uint8_t get_op(uint32_t inst) { return (inst >> 24) & 0xFF; }
inline uint8_t get_a(uint32_t inst) { return (inst >> 16) & 0xFF; }
inline uint8_t get_b(uint32_t inst) { return (inst >> 8) & 0xFF; }
inline uint8_t get_c(uint32_t inst) { return inst & 0xFF; }
inline uint16_t get_bx(uint32_t inst) { return inst & 0xFFFF; }
inline int16_t get_signed_bx(uint32_t inst) { return static_cast<int16_t>(inst & 0xFFFF); }

// Instruction sizes
static constexpr int INST_SIZE = 4;
static constexpr int WIDE_INST_SIZE = 8; // 1 WIDE prefix + 7 wide instruction

// Wide instruction encoding: [op:8][A:16][B:16][C:16] = 7 bytes
// Stored in a uint64_t (low 56 bits)
inline uint64_t make_wide_instruction(Opcode op, uint16_t a, uint16_t b, uint16_t c) {
    return (static_cast<uint64_t>(op_byte(op)) << 48) |
           (static_cast<uint64_t>(a) << 32) |
           (static_cast<uint64_t>(b) << 16) |
           static_cast<uint64_t>(c);
}
inline uint8_t get_wide_op(uint64_t inst) { return (inst >> 48) & 0xFF; }
inline uint16_t get_wide_a(uint64_t inst) { return (inst >> 32) & 0xFFFF; }
inline uint16_t get_wide_b(uint64_t inst) { return (inst >> 16) & 0xFFFF; }
inline uint16_t get_wide_c(uint64_t inst) { return inst & 0xFFFF; }

} // namespace akar
