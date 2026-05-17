#pragma once
#include "akar/common/value.h"
#include "akar/common/opcodes.h"
#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <unordered_map>

namespace akar {

// .ako file format:
// Header: "AKAR" (4 bytes) + version (2 bytes) + flags (2 bytes)
// Section table: count(4) + [type(1) + offset(4) + size(4)] * count
// Sections:
//   1. STRINGS: serialized string table
//   2. CONSTANTS: serialized constant pool
//   3. BYTECODE: raw bytecode
//   4. FUNCTIONS: function metadata (name, arity, register_count, bytecode offset/size)
//   5. ENTRY: entry point function index

constexpr uint32_t AKAR_MAGIC = 0x414B4152; // "AKAR"
constexpr uint16_t AKAR_VERSION = 1;

// Flag bits
constexpr uint16_t AKAR_FLAG_DEBUG_SYMBOLS = 0x0001;

// Symbol hash function (FNV-1a) — used by both writer and VM for consistent naming
inline uint32_t akar_fnv1a(const char* data, size_t len) {
    uint32_t hash = 0x811c9dc5;
    for (size_t i = 0; i < len; i++) {
        hash ^= static_cast<uint8_t>(data[i]);
        hash *= 0x01000193;
    }
    return hash;
}

inline std::string akar_hash_symbol(const std::string& s) {
    char buf[12];
    snprintf(buf, sizeof(buf), "s_%08x", akar_fnv1a(s.data(), s.size()));
    return std::string(buf);
}

enum class SectionType : uint8_t {
    Strings = 1,
    Constants = 2,
    Bytecode = 3,
    Functions = 4,
    Entry = 5,
};

struct SectionHeader {
    SectionType type;
    uint32_t offset;
    uint32_t size;
};

struct ObjectFileHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t section_count;
};

class ObjectFileWriter {
public:
    bool write(const std::string& path, ObjFunction* entry, bool strip_symbols = false,
               const std::set<std::string>& identifier_values = {},
               const std::set<std::string>& literal_values = {});

private:
    void write_header(std::ofstream& file);
    void write_section(std::ofstream& file, SectionType type, const std::vector<uint8_t>& data);
    std::vector<uint8_t> serialize_strings(const std::vector<std::string>& strings);
    std::vector<uint8_t> serialize_constants(const std::vector<Value>& constants);
    std::vector<uint8_t> serialize_functions(ObjFunction* entry);

    std::vector<std::string> collect_strings(ObjFunction* entry);
    std::vector<Value> collect_constants(ObjFunction* entry);
    void collect_all_strings(const std::vector<Value>& constants,
                             std::unordered_map<std::string, uint32_t>& indices);

    // State for current write
    bool strip_symbols_ = false;
    std::set<std::string> identifier_values_;
    std::set<std::string> literal_values_;
};

class ObjectFileReader {
public:
    ObjFunction* read(const std::string& path);

private:
    bool read_header(std::ifstream& file, ObjectFileHeader& header);
    SectionHeader find_section(std::ifstream& file, const ObjectFileHeader& header, SectionType type);
    std::vector<std::string> deserialize_strings(std::ifstream& file, const SectionHeader& section);
    std::vector<Value> deserialize_constants(std::ifstream& file, const SectionHeader& section,
                                              const std::vector<std::string>& strings);
};

} // namespace akar
