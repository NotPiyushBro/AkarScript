#include "akar/common/chunk.h"

namespace akar {

void Chunk::write(uint32_t instruction, int line) {
    code.push_back(instruction);
    lines.push_back(line);
}

size_t Chunk::add_constant(Value value) {
    constants.push_back(value);
    return constants.size() - 1;
}

} // namespace akar
