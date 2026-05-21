# Object File Format (.ako)

The `.ako` file format is a compact binary representation of compiled Akar bytecode.

## Header

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | magic | `0x414B4152` ("AKAR") |
| 4 | 2 | version | Format version (currently 2) |
| 6 | 2 | flags | Bit flags (see below) |
| 8 | 4 | section_count | Number of sections |

### Flags

| Bit | Name | Description |
|-----|------|-------------|
| 0 | `AKAR_FLAG_DEBUG_SYMBOLS` | File contains debug info |

## Section Table

After the header, `section_count` section entries:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | type | Section type (see below) |
| 1 | 4 | offset | Byte offset from file start |
| 5 | 4 | size | Section size in bytes |

## Section Types

| Type | Value | Description |
|------|-------|-------------|
| `Strings` | 1 | String table |
| `Constants` | 2 | Constant pool |
| `Bytecode` | 3 | Raw bytecode |
| `Functions` | 4 | Function metadata |
| `Entry` | 5 | Entry point function index |

## String Table (Section 1)

A sequence of length-prefixed strings:

```
[count:4] [len:4][data:len] [len:4][data:len] ...
```

## Constants (Section 2)

Serialized constant pool:

| Tag | Description | Data |
|-----|-------------|------|
| 0 | nil | (none) |
| 1 | bool | [value:1] |
| 2 | number | [f64:8] |
| 3,0 | string ref | [string_index:4] |
| 3,1 | function | (nested function data) |

## Bytecode (Section 3)

Raw bytecode bytes. Each instruction is 4 bytes (normal) or 8 bytes (WIDE prefix + wide instruction).

## Functions (Section 4)

Function metadata:

| Field | Size | Description |
|-------|------|-------------|
| name_len | 4 | Function name length |
| name | var | Function name |
| arity | 2 | Parameter count |
| register_count | 2 | Number of registers |
| has_varargs | 1 | Variadic function flag |
| upvalue_count | 4 | Number of upvalue descriptors |
| upvalues | var | [index:1, is_local:1] per upvalue |
| constant_count | 4 | Number of constants |
| constants | var | Serialized constants |
| bytecode_size | 4 | Bytecode size |
| bytecode | var | Bytecode bytes |

## Entry (Section 5)

Single 4-byte value: the index of the entry point function in the Functions section.

## Symbol Hashing

Identifiers in the constant pool are hashed using FNV-1a for efficient runtime lookup:

```c
uint32_t hash = 0x811c9dc5;
for (each byte in string) {
    hash ^= byte;
    hash *= 0x01000193;
}
```

Hashed names are stored as `s_%08x` (e.g., `s_2e2a4a6a`).

## Endianness

All multi-byte values are stored in **big-endian** (network byte order).

## Example

A simple `print(42)` program:

```
Header:
  magic:    41 4B 41 52    "AKAR"
  version:  00 02          v2
  flags:    00 00          no debug
  sections: 00 00 00 05    5 sections

Sections:
  Strings:  offset=12, size=...
  Constants: offset=..., size=...
  Bytecode:  offset=..., size=12
    00 00 00 00    LOAD_CONST R0, BX=0   (constant 0 = 42)
    2D 00 00 00    PRINT R0
    2E 00 00 00    HALT
  Functions: offset=..., size=...
  Entry:     00 00 00 00    entry = function 0
```
