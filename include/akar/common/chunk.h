#pragma once
#include "value.h"
#include "opcodes.h"
#include <vector>

namespace akar {

// Chunk is a unit of bytecode (a function body)
struct Chunk {
    std::vector<uint32_t> code;
    std::vector<Value> constants;
    std::vector<int> lines; // line number per instruction

    void write(uint32_t instruction, int line);
    size_t add_constant(Value value);
    size_t size() const { return code.size(); }
};

} // namespace akar
