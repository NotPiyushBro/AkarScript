# VM Internals

## Architecture Overview

```
Source (.ak) → Lexer → Parser → AST → CodeGenerator → Bytecode (ObjFunction)
                                                         ↓
                                              ObjectFileWriter → .ako binary
                                                         ↓
                                              VM (register-based interpreter)
```

## Register-Based VM

Akar uses a **register-based** bytecode VM (not stack-based like Lua or JVM).

### Instruction Format

**Normal format (4 bytes):**
```
[opcode:8] [A:8] [B:8] [C:8]
```

**Wide format (8 bytes, for registers > 255):**
```
[WIDE:8] [opcode:8] [A:16] [B:16] [C:16]
```

The `WIDE` prefix instruction (opcode 67) signals that the next instruction uses 16-bit register fields, allowing functions with up to 65535 registers.

### Register Allocation

The compiler allocates registers linearly:
- Registers 0..arity-1: function parameters
- Registers arity..N: local variables and temporaries
- `register_count` is stored per function

### NaN-Boxing

All values are stored in 8 bytes using IEEE 754 NaN-boxing:
- **Numbers**: stored as raw doubles
- **nil**: `0x7FFC000000000000`
- **true/false**: `0x7FF8000000000001` / `0x7FF8000000000002`
- **Object pointers**: tagged with bit 51 set, lower 48 bits = pointer

This means every `Value` fits in a single register (8 bytes) with no boxing overhead for numbers.

## Opcode Reference

### Load/Store

| Opcode | Byte | Format | Description |
|--------|------|--------|-------------|
| `LOAD_CONST` | 0 | `A = constants[BX]` | Load constant by 16-bit index |
| `LOAD_NIL` | 1 | `A = nil` | Load nil |
| `LOAD_TRUE` | 2 | `A = true` | Load true |
| `LOAD_FALSE` | 3 | `A = false` | Load false |
| `MOVE` | 4 | `A = R[B]` | Copy register |

### Variable Access

| Opcode | Byte | Format | Description |
|--------|------|--------|-------------|
| `GET_LOCAL` | 5 | `A = stack[BP + B]` | Read local |
| `SET_LOCAL` | 6 | `stack[BP + B] = A` | Write local |
| `GET_UPVALUE` | 7 | `A = upvalue[B]` | Read captured variable |
| `SET_UPVALUE` | 8 | `upvalue[B] = A` | Write captured variable |
| `GET_GLOBAL` | 9 | `A = globals[name_at_B]` | Read global |
| `SET_GLOBAL` | 10 | `globals[name_at_B] = A` | Write global |

### Arithmetic

| Opcode | Byte | Format | Description |
|--------|------|--------|-------------|
| `ADD` | 11 | `A = R[B] + R[C]` | Add (numbers or strings) |
| `SUB` | 12 | `A = R[B] - R[C]` | Subtract |
| `MUL` | 13 | `A = R[B] * R[C]` | Multiply |
| `DIV` | 14 | `A = R[B] / R[C]` | Divide |
| `MOD` | 15 | `A = R[B] % R[C]` | Modulo |
| `NEG` | 16 | `A = -R[B]` | Negate |

### Comparison

| Opcode | Byte | Format | Description |
|--------|------|--------|-------------|
| `EQ` | 17 | `A = R[B] == R[C]` | Equal |
| `NEQ` | 18 | `A = R[B] != R[C]` | Not equal |
| `LT` | 19 | `A = R[B] < R[C]` | Less than |
| `LTE` | 20 | `A = R[B] <= R[C]` | Less or equal |
| `GT` | 21 | `A = R[B] > R[C]` | Greater than |
| `GTE` | 22 | `A = R[B] >= R[C]` | Greater or equal |
| `NOT` | 23 | `A = !R[B]` | Logical not |

### Control Flow

| Opcode | Byte | Format | Description |
|--------|------|--------|-------------|
| `JMP` | 24 | `PC += signed_BX` | Unconditional jump |
| `JMP_IF_FALSE` | 25 | `if (!R[A]) PC += signed_BX` | Jump if false |
| `JMP_IF_TRUE` | 26 | `if (R[A]) PC += signed_BX` | Jump if true |

### Functions

| Opcode | Byte | Format | Description |
|--------|------|--------|-------------|
| `CALL` | 27 | `R[A] = R[A](R[A+1]..R[A+B])` | Call function, B = arg count |
| `CLOSURE` | 28 | `A = closure(constants[BX])` | Create closure with upvalues |
| `CLOSE_UPVALUE` | 29 | Close upvalues at depth A | Close captured variables |
| `RETURN` | 30 | `return R[A]` | Return from function |
| `TAIL_CALL` | 63 | Like CALL, reuses frame | Tail call optimization |

### Data Structures

| Opcode | Byte | Format | Description |
|--------|------|--------|-------------|
| `NEW_ARRAY` | 31 | `A = new array` | Create array |
| `NEW_MAP` | 32 | `A = new map` | Create map |
| `GET_INDEX` | 33 | `A = R[B][R[C]]` | Index access |
| `SET_INDEX` | 34 | `R[A][R[B]] = R[C]` | Index assignment |
| `GET_FIELD` | 35 | `A = R[B].field_C` | Field access |
| `SET_FIELD` | 36 | `R[A].field_B = R[C]` | Field assignment |

### Classes

| Opcode | Byte | Format | Description |
|--------|------|--------|-------------|
| `NEW_CLASS` | 37 | `A = new class(BX)` | Create class |
| `NEW_INSTANCE` | 38 | `A = new instance of R[B]` | Create instance |
| `GET_METHOD` | 39 | `A = R[B].method_C` | Get method |
| `INVOKE` | 40 | Combined GET_METHOD + CALL | Method call |

### Iterators

| Opcode | Byte | Format | Description |
|--------|------|--------|-------------|
| `ITER_INIT` | 42 | `A = iterator(R[B])` | Create iterator |
| `ITER_NEXT` | 43 | `A = iter_next(R[B])` | Get next value |
| `ITER_DONE` | 44 | `A = iter_done(R[B])` | Check if done |

### Quickened Opcodes (Runtime Specialized)

These are emitted by the VM at runtime when types are known:

| Opcode | Byte | Description |
|--------|------|-------------|
| `ADD_NUM` | 48 | Add (both numbers, no type check) |
| `SUB_NUM` | 49 | Subtract (both numbers) |
| `MUL_NUM` | 50 | Multiply (both numbers) |
| `DIV_NUM` | 51 | Divide (both numbers) |
| `MOD_NUM` | 52 | Modulo (both numbers) |
| `ADD_STR` | 53 | Concatenate (both strings) |
| `EQ_NUM` | 54 | Equal (both numbers) |
| `NEQ_NUM` | 55 | Not equal (both numbers) |
| `LT_NUM` | 56 | Less than (both numbers) |
| `LTE_NUM` | 57 | Less or equal (both numbers) |
| `GT_NUM` | 58 | Greater than (both numbers) |
| `GTE_NUM` | 59 | Greater or equal (both numbers) |
| `MOD_EQ_ZERO` | 60 | Fused: `(R[B] % R[C]) == 0` |

### Other

| Opcode | Byte | Description |
|--------|------|-------------|
| `PRINT` | 45 | Print R[A] |
| `HALT` | 46 | Stop execution |
| `NOP` | 47 | No operation |
| `FIBER_YIELD` | 61 | Yield from fiber |
| `FIBER_RESUME` | 62 | Resume fiber |
| `AWAIT` | 64 | Await (suspend if nil) |
| `THROW` | 65 | Throw exception |
| `TRY_BEGIN` | 66 | Start try block |
| `TRY_END` | 67 | End try block |
| `WIDE` | 68 | Prefix for 16-bit registers |

## Garbage Collector

### Tri-Color Mark-Sweep

Akar uses a tri-color mark-sweep GC with incremental marking:

- **White**: potentially dead (unmarked)
- **Gray**: marked but children not yet traced
- **Black**: marked and children traced

### Incremental Collection

The GC runs in three phases:

1. **Idle** → triggers when `allocated_bytes >= next_gc_threshold`
2. **Marking** → incremental: traces 64 gray objects per step (every 128 opcodes)
3. **Sweeping** → atomic: frees all white objects at once

The sweep is atomic to avoid collecting objects allocated during the sweep. After marking completes, roots are re-marked to catch objects allocated during incremental marking.

### GC Triggers

- Every 128 opcodes, `gc_step()` is called
- Threshold grows: `next_gc = allocated_bytes * 2` after each collection
- Memory limit: configurable via `akar_set_memory_limit()`

### What's a GC Root?

- VM stack (all active frames)
- Global variables
- Closures in call frames
- Open upvalues
- Interned string table
- Active fibers and yield/resume values

## Performance Features

### Computed Gotos

On GCC/Clang, the VM uses computed gotos (threaded dispatch) instead of a switch statement. Each opcode handler jumps directly to the next handler via a dispatch table.

### Quickening

When an operation first executes, the VM replaces the generic opcode with a type-specialized version. For example:
- `ADD` (checks types) → `ADD_NUM` (no check) after first number+number execution

This eliminates type checks in hot loops after the first iteration.

### Inline Caching

Globals use pointer-keyed hash maps (`ObjString*` keys). Since strings are interned, global lookup is a single pointer comparison rather than string hashing.

### Register-Based Design

Unlike stack-based VMs, register-based VMs don't need PUSH/POP for every operation. `A = B + C` is a single instruction instead of `PUSH B; PUSH C; ADD; POP A`.

### Tail Call Optimization

When the last action of a function is calling another function, `TAIL_CALL` reuses the current stack frame instead of allocating a new one. This prevents stack overflow in recursive patterns.
