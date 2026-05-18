#include "akar/vm/object_file.h"
#include <cstring>
#include <cstdio>
#include <unordered_map>

namespace akar {

// Use shared hash functions from object_file.h

bool ObjectFileWriter::write(const std::string& path, ObjFunction* entry, bool strip_symbols,
                             const std::set<std::string>& identifier_values,
                             const std::set<std::string>& literal_values) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    strip_symbols_ = strip_symbols;
    identifier_values_ = identifier_values;
    literal_values_ = literal_values;

    // Collect strings and constants
    auto strings = collect_strings(entry);
    auto constants = collect_constants(entry);

    // Serialize all section data
    auto strings_data = serialize_strings(strings);
    auto constants_data = serialize_constants(constants);
    auto bytecode_data = std::vector<uint8_t>(entry->bytecode.begin(), entry->bytecode.end());
    auto functions_data = serialize_functions(entry);
    std::vector<uint8_t> entry_data = {0, 0, 0, 0};

    // Build section table with correct offsets
    struct SectionEntry {
        SectionType type;
        std::vector<uint8_t>* data;
    };
    SectionEntry sections[] = {
        {SectionType::Strings,   &strings_data},
        {SectionType::Constants, &constants_data},
        {SectionType::Bytecode,  &bytecode_data},
        {SectionType::Functions, &functions_data},
        {SectionType::Entry,     &entry_data},
    };

    // Calculate offsets: data starts right after header + all section headers
    uint32_t data_offset = sizeof(ObjectFileHeader) + 5 * sizeof(SectionHeader);
    std::vector<SectionHeader> section_headers;
    for (auto& sec : sections) {
        SectionHeader sh;
        sh.type = sec.type;
        sh.offset = data_offset;
        sh.size = sec.data->size();
        section_headers.push_back(sh);
        data_offset += sec.data->size();
    }

    // Write header
    ObjectFileHeader header;
    header.magic = AKAR_MAGIC;
    header.version = AKAR_VERSION;
    header.flags = strip_symbols_ ? 0 : AKAR_FLAG_DEBUG_SYMBOLS;
    header.section_count = 5;
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write section table
    for (auto& sh : section_headers) {
        file.write(reinterpret_cast<const char*>(&sh), sizeof(sh));
    }

    // Write section data
    for (auto& sec : sections) {
        file.write(reinterpret_cast<const char*>(sec.data->data()), sec.data->size());
    }

    file.close();
    return true;
}

void ObjectFileWriter::write_header(std::ofstream& file) {
    // Already written in write()
}

void ObjectFileWriter::write_section(std::ofstream& file, SectionType type, const std::vector<uint8_t>& data) {
    // Unused now — kept for interface compatibility
}

std::vector<uint8_t> ObjectFileWriter::serialize_strings(const std::vector<std::string>& strings) {
    std::vector<uint8_t> data;
    // Count
    uint32_t count = strings.size();
    data.push_back((count >> 24) & 0xFF);
    data.push_back((count >> 16) & 0xFF);
    data.push_back((count >> 8) & 0xFF);
    data.push_back(count & 0xFF);

    for (auto& s : strings) {
        uint32_t len = s.size();
        data.push_back((len >> 24) & 0xFF);
        data.push_back((len >> 16) & 0xFF);
        data.push_back((len >> 8) & 0xFF);
        data.push_back(len & 0xFF);
        data.insert(data.end(), s.begin(), s.end());
    }
    return data;
}

// Helper to write big-endian integers
static void write_u8(std::vector<uint8_t>& data, uint8_t v) { data.push_back(v); }
static void write_u16(std::vector<uint8_t>& data, uint16_t v) {
    data.push_back((v >> 8) & 0xFF); data.push_back(v & 0xFF);
}
static void write_u32(std::vector<uint8_t>& data, uint32_t v) {
    data.push_back((v >> 24) & 0xFF); data.push_back((v >> 16) & 0xFF);
    data.push_back((v >> 8) & 0xFF); data.push_back(v & 0xFF);
}
static void write_f64(std::vector<uint8_t>& data, double v) {
    uint64_t bits; std::memcpy(&bits, &v, 8);
    for (int i = 7; i >= 0; i--) data.push_back((bits >> (i * 8)) & 0xFF);
}
static void write_str(std::vector<uint8_t>& data, const std::string& s) {
    write_u32(data, s.size());
    data.insert(data.end(), s.begin(), s.end());
}

// Serialize a single value (recursive for functions)
static void serialize_value(std::vector<uint8_t>& data, const Value& val,
                            const std::unordered_map<std::string, uint32_t>& string_indices,
                            bool strip_names) {
    if (val.is_nil()) {
        write_u8(data, 0); // Nil tag
    } else if (val.is_bool()) {
        write_u8(data, 1); // Bool tag
        write_u8(data, val.get_bool() ? 1 : 0);
    } else if (val.is_number()) {
        write_u8(data, 2); // Number tag
        write_f64(data, val.get_number());
    } else if (val.is_obj()) {
        write_u8(data, 3); // Obj tag
        if (val.is_string()) {
            write_u8(data, 0); // string sub-tag
            write_u32(data, string_indices.at(val.as_string()->value));
        } else if (val.is_function()) {
            write_u8(data, 1); // function sub-tag
            auto* func = val.as_function();
            write_str(data, strip_names ? akar_hash_symbol(func->name) : func->name);
            write_u16(data, func->arity);
            write_u16(data, func->register_count);
            write_u8(data, func->has_varargs ? 1 : 0);
            write_u32(data, func->upvalue_descs.size());
            for (auto& desc : func->upvalue_descs) {
                write_u8(data, desc.index);
                write_u8(data, desc.is_local ? 1 : 0);
            }
            // Serialize function's own constants recursively
            write_u32(data, func->constants.size());
            for (auto& c : func->constants) {
                serialize_value(data, c, string_indices, strip_names);
            }
            // Bytecode
            write_u32(data, func->bytecode.size());
            data.insert(data.end(), func->bytecode.begin(), func->bytecode.end());
        } else {
            // Unsupported object type (array, map, closure, class, etc.)
            // Write nil sub-tag to keep stream consistent
            data.pop_back(); // remove the OBJ tag we already wrote
            write_u8(data, 0); // Nil tag
        }
    }
}

std::vector<uint8_t> ObjectFileWriter::serialize_constants(const std::vector<Value>& constants) {
    // Build string index lookup (deduplicated, first-seen order)
    std::unordered_map<std::string, uint32_t> string_indices;
    collect_all_strings(constants, string_indices);

    std::vector<uint8_t> data;
    bool strip_names = strip_symbols_;
    write_u32(data, constants.size());

    for (auto& val : constants) {
        serialize_value(data, val, string_indices, strip_names);
    }
    return data;
}

void ObjectFileWriter::collect_all_strings(const std::vector<Value>& constants,
                                            std::unordered_map<std::string, uint32_t>& indices) {
    for (auto& val : constants) {
        if (val.is_string()) {
            const std::string& s = val.as_string()->value;
            if (indices.find(s) == indices.end()) {
                indices[s] = indices.size();
            }
        } else if (val.is_function()) {
            collect_all_strings(val.as_function()->constants, indices);
        }
    }
}

std::vector<uint8_t> ObjectFileWriter::serialize_functions(ObjFunction* entry) {
    std::vector<uint8_t> data;
    // Single function for now
    std::string name = strip_symbols_ ? akar_hash_symbol(entry->name) : entry->name;
    uint32_t name_len = name.size();
    data.push_back((name_len >> 24) & 0xFF);
    data.push_back((name_len >> 16) & 0xFF);
    data.push_back((name_len >> 8) & 0xFF);
    data.push_back(name_len & 0xFF);
    data.insert(data.end(), name.begin(), name.end());

    uint16_t arity = entry->arity;
    data.push_back((arity >> 8) & 0xFF);
    data.push_back(arity & 0xFF);

    uint16_t regs = entry->register_count;
    data.push_back((regs >> 8) & 0xFF);
    data.push_back(regs & 0xFF);

    data.push_back(entry->has_varargs ? 1 : 0);

    return data;
}

std::vector<std::string> ObjectFileWriter::collect_strings(ObjFunction* entry) {
    std::unordered_map<std::string, uint32_t> indices;
    collect_all_strings(entry->constants, indices);

    // Return in index order, optionally hashing only identifiers (not literals)
    std::vector<std::string> result(indices.size());
    for (auto& [s, i] : indices) {
        bool is_identifier = identifier_values_.count(s) > 0;
        bool is_literal = literal_values_.count(s) > 0;
        result[i] = (strip_symbols_ && is_identifier && !is_literal) ? akar_hash_symbol(s) : s;
    }
    return result;
}

std::vector<Value> ObjectFileWriter::collect_constants(ObjFunction* entry) {
    return entry->constants;
}

ObjFunction* ObjectFileReader::read(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return nullptr;

    ObjectFileHeader header;
    if (!read_header(file, header)) return nullptr;

    if (header.magic != AKAR_MAGIC || header.version != AKAR_VERSION) {
        return nullptr;
    }

    // Read sections
    auto strings_section = find_section(file, header, SectionType::Strings);
    auto constants_section = find_section(file, header, SectionType::Constants);
    auto bytecode_section = find_section(file, header, SectionType::Bytecode);
    auto functions_section = find_section(file, header, SectionType::Functions);

    auto strings = deserialize_strings(file, strings_section);
    auto constants = deserialize_constants(file, constants_section, strings);

    auto* func = allocate_function();
    func->constants = constants;

    // Read bytecode
    file.seekg(bytecode_section.offset);
    func->bytecode.resize(bytecode_section.size);
    file.read(reinterpret_cast<char*>(func->bytecode.data()), bytecode_section.size);

    // Read function metadata from functions section
    if (functions_section.size > 0) {
        file.seekg(functions_section.offset);

        // Read name
        uint8_t name_len_bytes[4];
        file.read(reinterpret_cast<char*>(name_len_bytes), 4);
        uint32_t name_len = (name_len_bytes[0] << 24) | (name_len_bytes[1] << 16) |
                            (name_len_bytes[2] << 8) | name_len_bytes[3];
        std::string name(name_len, '\0');
        file.read(&name[0], name_len);
        func->name = name;

        // Read arity
        uint8_t arity_bytes[2];
        file.read(reinterpret_cast<char*>(arity_bytes), 2);
        func->arity = (arity_bytes[0] << 8) | arity_bytes[1];

        // Read register count
        uint8_t regs_bytes[2];
        file.read(reinterpret_cast<char*>(regs_bytes), 2);
        func->register_count = (regs_bytes[0] << 8) | regs_bytes[1];

        // Read has_varargs
        uint8_t varargs_byte;
        file.read(reinterpret_cast<char*>(&varargs_byte), 1);
        func->has_varargs = (varargs_byte != 0);
    }

    file.close();
    return func;
}

bool ObjectFileReader::read_header(std::ifstream& file, ObjectFileHeader& header) {
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    return file.good();
}

SectionHeader ObjectFileReader::find_section(std::ifstream& file, const ObjectFileHeader& header, SectionType type) {
    SectionHeader section;
    // Seek to start of section table (right after the header)
    file.seekg(sizeof(ObjectFileHeader));
    // Read section table
    for (uint32_t i = 0; i < header.section_count; i++) {
        file.read(reinterpret_cast<char*>(&section), sizeof(section));
        if (section.type == type) return section;
    }
    section.type = type;
    section.offset = 0;
    section.size = 0;
    return section;
}

// Maximum allowed size for any single allocation from a file (16 MB)
static constexpr uint32_t MAX_FILE_ALLOC = 16 * 1024 * 1024;

// Helper to read big-endian integers with error checking
static bool read_bytes(std::ifstream& file, void* buf, size_t n) {
    file.read(reinterpret_cast<char*>(buf), n);
    return file.good();
}
static uint8_t read_u8(std::ifstream& file, bool* ok = nullptr) {
    uint8_t v; if (!read_bytes(file, &v, 1)) { if (ok) *ok = false; return 0; } return v;
}
static uint16_t read_u16(std::ifstream& file, bool* ok = nullptr) {
    uint8_t b[2]; if (!read_bytes(file, b, 2)) { if (ok) *ok = false; return 0; }
    return (b[0] << 8) | b[1];
}
static uint32_t read_u32(std::ifstream& file, bool* ok = nullptr) {
    uint8_t b[4]; if (!read_bytes(file, b, 4)) { if (ok) *ok = false; return 0; }
    return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}
static double read_f64(std::ifstream& file, bool* ok = nullptr) {
    uint8_t b[8]; if (!read_bytes(file, b, 8)) { if (ok) *ok = false; return 0.0; }
    uint64_t bits = 0;
    for (int j = 0; j < 8; j++) bits = (bits << 8) | b[j];
    double num; std::memcpy(&num, &bits, 8); return num;
}
static std::string read_str(std::ifstream& file, bool* ok = nullptr) {
    uint32_t len = read_u32(file, ok);
    if (ok && !*ok) return "";
    if (len > MAX_FILE_ALLOC) { if (ok) *ok = false; return ""; }
    std::string s(len, '\0');
    if (!read_bytes(file, &s[0], len)) { if (ok) *ok = false; return ""; }
    return s;
}

std::vector<std::string> ObjectFileReader::deserialize_strings(std::ifstream& file, const SectionHeader& section) {
    std::vector<std::string> result;
    if (section.size == 0) return result;
    file.seekg(section.offset);

    bool ok = true;
    uint32_t count = read_u32(file, &ok);
    if (!ok || count > MAX_FILE_ALLOC) return result;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t len = read_u32(file, &ok);
        if (!ok || len > MAX_FILE_ALLOC) return result;
        std::string s(len, '\0');
        if (!read_bytes(file, &s[0], len)) return result;
        result.push_back(std::move(s));
    }
    return result;
}

static Value deserialize_value(std::ifstream& file, const std::vector<std::string>& strings) {
    bool ok = true;
    uint8_t type = read_u8(file, &ok);
    if (!ok) return Value();
    switch (type) {
        case 0: return Value(); // Nil
        case 1: return Value(read_u8(file, &ok) != 0); // Bool
        case 2: return Value(read_f64(file, &ok)); // Number
        case 3: { // Obj
            uint8_t subtag = read_u8(file, &ok);
            if (!ok) return Value();
            if (subtag == 0) { // string
                uint32_t idx = read_u32(file, &ok);
                if (!ok || idx >= strings.size()) return Value();
                return Value(static_cast<Obj*>(get_string_table().intern(strings[idx])));
            } else if (subtag == 1) { // function
                auto* func = allocate_function();
                func->name = read_str(file, &ok);
                if (!ok) return Value();
                func->arity = read_u16(file, &ok);
                func->register_count = read_u16(file, &ok);
                func->has_varargs = (read_u8(file, &ok) != 0);
                if (!ok) return Value();
                uint32_t uv_count = read_u32(file, &ok);
                if (!ok || uv_count > MAX_FILE_ALLOC) return Value();
                for (uint32_t i = 0; i < uv_count; i++) {
                    UpvalueDesc desc;
                    desc.index = read_u8(file, &ok);
                    desc.is_local = read_u8(file, &ok) != 0;
                    if (!ok) return Value();
                    func->upvalue_descs.push_back(desc);
                }
                uint32_t const_count = read_u32(file, &ok);
                if (!ok || const_count > MAX_FILE_ALLOC) return Value();
                for (uint32_t i = 0; i < const_count; i++) {
                    func->constants.push_back(deserialize_value(file, strings));
                }
                uint32_t bc_size = read_u32(file, &ok);
                if (!ok || bc_size > MAX_FILE_ALLOC) return Value();
                func->bytecode.resize(bc_size);
                if (!read_bytes(file, func->bytecode.data(), bc_size)) return Value();
                return Value(static_cast<Obj*>(func));
            }
            return Value();
        }
        default: return Value();
    }
}

std::vector<Value> ObjectFileReader::deserialize_constants(std::ifstream& file, const SectionHeader& section,
                                                            const std::vector<std::string>& strings) {
    std::vector<Value> result;
    if (section.size == 0) return result;
    file.seekg(section.offset);

    uint32_t count = read_u32(file);
    for (uint32_t i = 0; i < count; i++) {
        result.push_back(deserialize_value(file, strings));
    }
    return result;
}

} // namespace akar
