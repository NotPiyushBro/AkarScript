// Akar Script — Platform-independent JIT Compiler
// Uses JITBackend abstract interface for code generation.
// No platform-specific code here.

#include "akar/vm/jit.h"
#include "akar/vm/vm.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sys/mman.h>

namespace akar {

// ============================================================
// JITCode destructor
// ============================================================
JITCode::~JITCode() {
    if (memory) {
        munmap(memory, size);
        memory = nullptr;
    }
}

// ============================================================
// Helper functions called from JIT code
// ============================================================

static constexpr int64_t NIL_BITS   = 0x7FFC000000000000LL;
static constexpr int64_t FALSE_BITS = 0x7FF8000000000001LL;
static constexpr int64_t TRUE_BITS  = 0x7FF8000000000002LL;
static constexpr uint64_t NAN_BASE  = 0x7FF8000000000000ULL;

static VM* g_jit_vm = nullptr;

void jit_set_vm(VM* vm) { g_jit_vm = vm; }

// --- Utility ---
static inline int64_t value_to_bits(Value v) {
    uint64_t bits;
    std::memcpy(&bits, &v, 8);
    return static_cast<int64_t>(bits);
}
static inline Value bits_to_value(int64_t bits) {
    Value v;
    std::memcpy(&v, &bits, 8);
    return v;
}

// --- Truthiness ---
static int64_t jit_is_truthy(int64_t bits) {
    uint64_t b = static_cast<uint64_t>(bits);
    if (b == FALSE_BITS || b == NIL_BITS) return FALSE_BITS;
    if (b == TRUE_BITS) return TRUE_BITS;
    if ((b & NAN_BASE) == NAN_BASE && (b & 0x0008000000000000ULL)) return TRUE_BITS;
    if ((b & NAN_BASE) != NAN_BASE) return TRUE_BITS;
    return FALSE_BITS;
}

// --- Mod helpers ---
static int64_t jit_fmod(double a, double b) {
    double r = std::fmod(a, b);
    return value_to_bits(Value(r));
}

static int64_t jit_mod_eq_zero(double a, double b) {
    return std::fmod(a, b) == 0.0 ? TRUE_BITS : FALSE_BITS;
}

// --- Bitwise helpers (operate on NaN-boxed doubles ↔ int64_t) ---
static int64_t jit_bit_and(int64_t a, int64_t b) {
    double da = bits_to_value(a).get_number();
    double db = bits_to_value(b).get_number();
    return value_to_bits(Value(static_cast<double>(
        static_cast<int64_t>(da) & static_cast<int64_t>(db))));
}
static int64_t jit_bit_or(int64_t a, int64_t b) {
    double da = bits_to_value(a).get_number();
    double db = bits_to_value(b).get_number();
    return value_to_bits(Value(static_cast<double>(
        static_cast<int64_t>(da) | static_cast<int64_t>(db))));
}
static int64_t jit_bit_xor(int64_t a, int64_t b) {
    double da = bits_to_value(a).get_number();
    double db = bits_to_value(b).get_number();
    return value_to_bits(Value(static_cast<double>(
        static_cast<int64_t>(da) ^ static_cast<int64_t>(db))));
}
static int64_t jit_bit_not(int64_t a) {
    double da = bits_to_value(a).get_number();
    return value_to_bits(Value(static_cast<double>(~static_cast<int64_t>(da))));
}
static int64_t jit_shl(int64_t a, int64_t b) {
    double da = bits_to_value(a).get_number();
    double db = bits_to_value(b).get_number();
    return value_to_bits(Value(static_cast<double>(
        static_cast<int64_t>(da) << static_cast<int64_t>(db))));
}
static int64_t jit_shr(int64_t a, int64_t b) {
    double da = bits_to_value(a).get_number();
    double db = bits_to_value(b).get_number();
    return value_to_bits(Value(static_cast<double>(
        static_cast<int64_t>(da) >> static_cast<int64_t>(db))));
}

// --- Upvalue helpers ---
static int64_t jit_get_upvalue(ObjClosure* closure, int index) {
    if (!closure || index >= (int)closure->upvalues.size()) return NIL_BITS;
    return value_to_bits(*closure->upvalues[index]->location);
}
static void jit_set_upvalue(ObjClosure* closure, int index, int64_t bits) {
    if (!closure || index >= (int)closure->upvalues.size()) return;
    *closure->upvalues[index]->location = bits_to_value(bits);
}

// --- Global helpers ---
static int64_t jit_get_global(ObjString* name) {
    if (!g_jit_vm || !name) return NIL_BITS;
    Value* val = g_jit_vm->jit_find_global(name);
    if (!val) return NIL_BITS;
    return value_to_bits(*val);
}
static void jit_set_global(ObjString* name, int64_t bits) {
    if (!g_jit_vm || !name) return;
    g_jit_vm->jit_set_global_val(name, bits_to_value(bits));
}

// --- Call helper ---
static int64_t jit_call_helper(Value* callee_slot, int arg_count) {
    Value callee = *callee_slot;
    if (!g_jit_vm) return NIL_BITS;
    if (callee.is_closure()) {
        ObjClosure* closure = callee.as_closure();
        Value* args = callee_slot + 1;
        Value result = g_jit_vm->jit_call_direct(closure, args, arg_count);
        *callee_slot = result;
        return value_to_bits(result);
    }
    if (callee.is_native()) {
        // Call native function directly
        ObjNative* native = callee.as_native();
        Value* args = callee_slot + 1;
        Value result = native->function(arg_count, args);
        *callee_slot = result;
        return value_to_bits(result);
    }
    return NIL_BITS;
}

// --- Array/Map helpers ---
static int64_t jit_new_array(Value* elements, int count) {
    auto* arr = allocate_array();
    for (int i = 0; i < count; i++) arr->elements.push_back(elements[i]);
    return value_to_bits(Value(static_cast<Obj*>(arr)));
}
static int64_t jit_new_map() {
    auto* map = allocate_map();
    return value_to_bits(Value(static_cast<Obj*>(map)));
}
static int64_t jit_get_index(int64_t obj_bits, int64_t idx_bits) {
    Value obj = bits_to_value(obj_bits);
    Value idx = bits_to_value(idx_bits);
    if (obj.is_array() && idx.is_number()) {
        auto* arr = obj.as_array();
        int i = static_cast<int>(idx.get_number());
        if (i < 0 || i >= (int)arr->elements.size()) return NIL_BITS;
        return value_to_bits(arr->elements[i]);
    }
    if (obj.is_map() && idx.is_string()) {
        auto* map = obj.as_map();
        auto it = map->entries.find(idx.as_string()->value);
        if (it == map->entries.end()) return NIL_BITS;
        return value_to_bits(it->second);
    }
    if (obj.is_string() && idx.is_number()) {
        auto* str = obj.as_string();
        int i = static_cast<int>(idx.get_number());
        if (i < 0 || i >= (int)str->value.size()) return NIL_BITS;
        auto* ch = get_string_table().intern(std::string(1, str->value[i]));
        return value_to_bits(Value(static_cast<Obj*>(ch)));
    }
    return NIL_BITS;
}
static void jit_set_index(int64_t obj_bits, int64_t idx_bits, int64_t val_bits) {
    Value obj = bits_to_value(obj_bits);
    Value idx = bits_to_value(idx_bits);
    Value val = bits_to_value(val_bits);
    if (obj.is_array() && idx.is_number()) {
        auto* arr = obj.as_array();
        int i = static_cast<int>(idx.get_number());
        if (i >= 0 && i < (int)arr->elements.size()) arr->elements[i] = val;
    }
    if (obj.is_map() && idx.is_string()) {
        auto* map = obj.as_map();
        map->entries[idx.as_string()->value] = val;
    }
}

// --- Field helpers ---
static int64_t jit_get_field(int64_t obj_bits, ObjString* name) {
    Value obj = bits_to_value(obj_bits);
    if (obj.is_instance()) {
        auto* inst = obj.as_instance();
        auto it = inst->fields.find(name->value);
        if (it != inst->fields.end()) return value_to_bits(it->second);
        // Check getter
        auto mit = inst->klass->methods.find(name->value);
        if (mit != inst->klass->methods.end()) return value_to_bits(mit->second);
    }
    if (obj.is_array()) {
        if (name->value == "length")
            return value_to_bits(Value(static_cast<double>(obj.as_array()->elements.size())));
    }
    if (obj.is_string()) {
        if (name->value == "length")
            return value_to_bits(Value(static_cast<double>(obj.as_string()->value.size())));
    }
    if (obj.is_map()) {
        if (name->value == "length")
            return value_to_bits(Value(static_cast<double>(obj.as_map()->entries.size())));
    }
    return NIL_BITS;
}
static void jit_set_field(int64_t obj_bits, ObjString* name, int64_t val_bits) {
    Value obj = bits_to_value(obj_bits);
    if (obj.is_instance()) {
        obj.as_instance()->fields[name->value] = bits_to_value(val_bits);
    }
}

// --- Class/Object helpers ---
static int64_t jit_new_class(ObjString* name) {
    auto* klass = allocate_class(name->value);
    return value_to_bits(Value(static_cast<Obj*>(klass)));
}
static int64_t jit_new_instance(int64_t klass_bits) {
    Value klass_val = bits_to_value(klass_bits);
    if (!klass_val.is_class()) return NIL_BITS;
    auto* inst = allocate_instance(klass_val.as_class());
    // Call init if exists
    auto it = klass_val.as_class()->methods.find("init");
    if (it != klass_val.as_class()->methods.end()) {
        // Store init method for later CALL
    }
    return value_to_bits(Value(static_cast<Obj*>(inst)));
}
static int64_t jit_get_method(int64_t obj_bits, ObjString* name) {
    Value obj = bits_to_value(obj_bits);
    if (obj.is_instance()) {
        auto it = obj.as_instance()->klass->methods.find(name->value);
        if (it != obj.as_instance()->klass->methods.end()) return value_to_bits(it->second);
    }
    if (obj.is_class()) {
        auto it = obj.as_class()->methods.find(name->value);
        if (it != obj.as_class()->methods.end()) return value_to_bits(it->second);
    }
    // Array/string built-in methods
    if (obj.is_array() || obj.is_string() || obj.is_map()) {
        return value_to_bits(obj); // placeholder
    }
    return NIL_BITS;
}

// --- Iterator helpers ---
static int64_t jit_new_range(int64_t start_bits, int64_t end_bits) {
    auto* it = allocate_iterator();
    it->kind = ObjIterator::RangeIter;
    it->range_current = bits_to_value(start_bits).get_number();
    it->range_end = bits_to_value(end_bits).get_number();
    it->range_step = 1.0;
    it->done = (it->range_current >= it->range_end);
    return value_to_bits(Value(static_cast<Obj*>(it)));
}
static int64_t jit_iter_init(int64_t obj_bits) {
    Value obj = bits_to_value(obj_bits);
    if (obj.is_array()) {
        auto* it = allocate_iterator();
        it->kind = ObjIterator::ArrayIter;
        it->arr = obj.as_array();
        it->arr_index = 0;
        it->done = it->arr->elements.empty();
        return value_to_bits(Value(static_cast<Obj*>(it)));
    }
    if (obj.is_string()) {
        auto* it = allocate_iterator();
        it->kind = ObjIterator::StringIter;
        it->str = obj.as_string();
        it->str_index = 0;
        it->done = it->str->value.empty();
        return value_to_bits(Value(static_cast<Obj*>(it)));
    }
    if (obj.is_iterator()) {
        return obj_bits; // already an iterator (range)
    }
    return NIL_BITS;
}
static int64_t jit_iter_next(int64_t iter_bits) {
    Value iter_val = bits_to_value(iter_bits);
    if (!iter_val.is_iterator()) return NIL_BITS;
    auto* it = iter_val.as_iterator();
    Value result;
    switch (it->kind) {
        case ObjIterator::ArrayIter:
            if (it->arr_index < (int)it->arr->elements.size())
                result = it->arr->elements[it->arr_index++];
            it->done = (it->arr_index >= (int)it->arr->elements.size());
            break;
        case ObjIterator::RangeIter:
            result = Value(it->range_current);
            it->range_current += it->range_step;
            it->done = (it->range_current >= it->range_end);
            break;
        case ObjIterator::StringIter:
            if (it->str_index < (int)it->str->value.size()) {
                auto* ch = get_string_table().intern(std::string(1, it->str->value[it->str_index]));
                result = Value(static_cast<Obj*>(ch));
            }
            it->str_index++;
            it->done = (it->str_index >= (int)it->str->value.size());
            break;
    }
    return value_to_bits(result);
}
static int64_t jit_iter_done(int64_t iter_bits) {
    Value iter_val = bits_to_value(iter_bits);
    if (!iter_val.is_iterator()) return TRUE_BITS;
    return iter_val.as_iterator()->done ? TRUE_BITS : FALSE_BITS;
}

// --- Print helper ---
static void jit_print(int64_t bits) {
    Value v = bits_to_value(bits);
    printf("%s\n", v.to_string().c_str());
}

// --- Signal/Effect helpers ---
static int64_t jit_signal_create(int64_t initial_bits, ObjString* name) {
    Value initial = bits_to_value(initial_bits);
    auto* sig = allocate_signal(initial, name ? name->value : "");
    return value_to_bits(Value(static_cast<Obj*>(sig)));
}
static int64_t jit_signal_get(int64_t signal_bits) {
    Value sv = bits_to_value(signal_bits);
    if (!sv.is_signal()) return NIL_BITS;
    // Track dependency if in effect
    if (g_jit_vm && g_jit_vm->current_effect_) {
        auto* sig = sv.as_signal();
        auto* eff = g_jit_vm->current_effect_;
        sig->subscribers.push_back(eff);
        eff->dependencies.push_back(sig);
    }
    return value_to_bits(sv.as_signal()->value);
}
static void jit_signal_set(int64_t signal_bits, int64_t val_bits) {
    Value sv = bits_to_value(signal_bits);
    if (!sv.is_signal()) return;
    auto* sig = sv.as_signal();
    sig->value = bits_to_value(val_bits);
    sig->write_generation++;
    // Queue effects
    if (g_jit_vm) {
        g_jit_vm->write_generation_++;
        for (auto* eff : sig->subscribers) {
            if (eff->state != ObjEffect::State::Queued) {
                eff->state = ObjEffect::State::Queued;
                g_jit_vm->effect_queue_.push_back(eff);
            }
        }
    }
}
static int64_t jit_effect_create(int64_t body_bits, ObjString* name) {
    Value body_val = bits_to_value(body_bits);
    if (!body_val.is_closure()) return NIL_BITS;
    auto* eff = allocate_effect(body_val.as_closure(), name ? name->value : "");
    return value_to_bits(Value(static_cast<Obj*>(eff)));
}
static void jit_effect_run(int64_t effect_bits) {
    Value ev = bits_to_value(effect_bits);
    if (!ev.is_effect() || !g_jit_vm) return;
    auto* eff = ev.as_effect();
    if (eff->state != ObjEffect::State::Queued) {
        eff->state = ObjEffect::State::Queued;
        g_jit_vm->effect_queue_.push_back(eff);
    }
}

// --- Enum helpers ---
static int64_t jit_enum_create(ObjString* name) {
    if (!g_jit_vm) return NIL_BITS;
    auto* klass = allocate_class(name->value);
    g_jit_vm->enum_type_counter_++;
    // Store enum type ID as a method (using double value)
    klass->methods["__enum_type_id"] = Value(static_cast<double>(g_jit_vm->enum_type_counter_));
    return value_to_bits(Value(static_cast<Obj*>(klass)));
}
static void jit_enum_variant(int64_t klass_bits, ObjString* variant_name, int64_t value_bits) {
    Value kv = bits_to_value(klass_bits);
    if (!kv.is_class()) return;
    kv.as_class()->methods[variant_name->value] = bits_to_value(value_bits);
}
static void jit_enum_data_variant(int64_t klass_bits, ObjString* variant_name) {
    Value kv = bits_to_value(klass_bits);
    if (!kv.is_class()) return;
    // Create a factory method that wraps data
    auto* func = allocate_function();
    func->arity = 1;
    func->name = variant_name->value;
    func->register_count = 4;
    // Store as method - result is a data-carrying instance
    auto* inst = allocate_instance(kv.as_class());
    // Store variant name as a string value in methods
    auto* variant_str = allocate_string(variant_name->value);
    inst->fields["__variant"] = Value(static_cast<Obj*>(variant_str));
    // Actually store a native factory
    kv.as_class()->methods[variant_name->value] = Value(static_cast<Obj*>(inst));
}
static int64_t jit_enum_get(int64_t obj_bits, ObjString* variant_name) {
    Value obj = bits_to_value(obj_bits);
    if (obj.is_instance()) {
        auto it = obj.as_instance()->fields.find(variant_name->value);
        if (it != obj.as_instance()->fields.end()) return value_to_bits(it->second);
    }
    return NIL_BITS;
}
static int64_t jit_enum_is(int64_t obj_bits, ObjString* enum_name) {
    // Simplified check
    return (bits_to_value(obj_bits).is_instance() || is_enum_value(bits_to_value(obj_bits))) ? TRUE_BITS : FALSE_BITS;
}

// ============================================================
// JITCompiler
// ============================================================

JITCompiler::JITCompiler() {
    backend_ = create_jit_backend();
}

JITCompiler::~JITCompiler() = default;

void JITCompiler::emit_bailout(int pc) {
    auto* b = backend_.get();
    b->emit_load_imm64(b->scratch0(), static_cast<uint64_t>(pc));
    b->emit_store_int(b->scratch0(), b->reg_outpc(), 0);
    b->emit_set_return(1);
    b->emit_epilogue();
}

void JITCompiler::emit_helper_call(void* func_addr) {
    backend_->emit_call_indirect(func_addr);
}

bool JITCompiler::compile_instruction(int& pc) {
    auto* b = backend_.get();
    int start_pc = pc;

    if (pc + 4 > static_cast<int>(bytecode_.size())) {
        emit_bailout(pc);
        return false;
    }

    uint8_t op = bytecode_[pc];
    uint8_t a  = bytecode_[pc + 1];
    uint8_t b8  = bytecode_[pc + 2];
    uint8_t c8  = bytecode_[pc + 3];
    uint16_t bx = (static_cast<uint16_t>(b8) << 8) | c8;
    int16_t sbx = static_cast<int16_t>(bx);
    pc += 4;

    int R0 = b->scratch0();
    int R1 = b->scratch1();
    int D0 = b->fscratch0();
    int D1 = b->fscratch1();
    int D2 = 2; // extra FP scratch (caller-saved on ARM64)
    int D3 = 3; // extra FP scratch

    switch (static_cast<Opcode>(op)) {

    // ================================================================
    // LOADS
    // ================================================================

    case Opcode::LOAD_IMM: {
        double d = static_cast<double>(b8);
        uint64_t bits;
        std::memcpy(&bits, &d, 8);
        b->emit_load_imm64(R0, bits);
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::LOAD_CONST: {
        b->emit_load_int(R0, b->reg_const(), bx * 8);
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::LOAD_NIL: {
        b->emit_load_imm64(R0, static_cast<uint64_t>(NIL_BITS));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::LOAD_TRUE: {
        b->emit_load_imm64(R0, static_cast<uint64_t>(TRUE_BITS));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::LOAD_FALSE: {
        b->emit_load_imm64(R0, static_cast<uint64_t>(FALSE_BITS));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::MOVE: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    // ================================================================
    // LOCAL VARIABLE ACCESS (same as MOVE, just alias)
    // ================================================================

    case Opcode::GET_LOCAL: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::SET_LOCAL: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        b->emit_store_int(R0, b->reg_base(), slot_offset(b8));
        break;
    }

    // ================================================================
    // UPVALUE ACCESS
    // ================================================================

    case Opcode::GET_UPVALUE: {
        // jit_get_upvalue(closure, index) → bits in X0
        b->emit_mov(R0, b->reg_closure());
        b->emit_load_imm64(R1, static_cast<uint64_t>(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_get_upvalue));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::SET_UPVALUE: {
        // jit_set_upvalue(closure, index, bits)
        b->emit_mov(R0, b->reg_closure());
        b->emit_load_imm64(R1, static_cast<uint64_t>(b8));
        b->emit_load_int(2, b->reg_base(), slot_offset(a)); // X2 = value bits
        emit_helper_call(reinterpret_cast<void*>(&jit_set_upvalue));
        break;
    }

    // ================================================================
    // GLOBAL ACCESS
    // ================================================================

    case Opcode::GET_GLOBAL: {
        // Load ObjString* name from constants[bx]
        b->emit_load_int(R0, b->reg_const(), bx * 8); // X0 = name ObjString*
        emit_helper_call(reinterpret_cast<void*>(&jit_get_global));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::SET_GLOBAL: {
        // jit_set_global(name, bits)
        b->emit_load_int(R0, b->reg_const(), bx * 8); // X0 = name
        b->emit_load_int(R1, b->reg_base(), slot_offset(a)); // X1 = value bits
        emit_helper_call(reinterpret_cast<void*>(&jit_set_global));
        break;
    }

    // ================================================================
    // ARITHMETIC (type-specialized)
    // ================================================================

    case Opcode::ADD_NUM: {
        invalidate_fp_cache();
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        // Small int guard
        b->emit_lsr_imm(2, R0, 48);
        b->emit_load_imm64(3, 0xFFF7);
        b->emit_cmp(2, 3);
        size_t add_smi_bail_a = b->emit_branch_cond(b->cond_ne(), 0);
        b->emit_lsr_imm(2, R1, 48);
        b->emit_cmp(2, 3);
        size_t add_smi_bail_b = b->emit_branch_cond(b->cond_ne(), 0);
        b->emit_lsl(4, R0, 16); b->emit_asr_imm(4, 4, 16);
        b->emit_lsl(5, R1, 16); b->emit_asr_imm(5, 5, 16);
        b->emit_add(4, 4, 5);
        b->emit_lsl(4, 4, 16); b->emit_lsr(4, 4, 16);
        b->emit_load_imm64(3, Value::SMALLINT_TAG);
        b->emit_orr(4, 4, 3);
        b->emit_store_int(4, b->reg_base(), slot_offset(a));
        size_t add_smi_done = b->emit_branch(0);
        b->patch_branch(add_smi_bail_a, b->code_size());
        b->patch_branch(add_smi_bail_b, b->code_size());
        b->emit_fmov_from_int(D0, R0);
        b->emit_fmov_from_int(D1, R1);
        b->emit_fadd(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        b->patch_branch(add_smi_done, b->code_size());
        break;
    }
    case Opcode::SUB_NUM: {
        invalidate_fp_cache();
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        b->emit_lsr_imm(2, R0, 48);
        b->emit_load_imm64(3, 0xFFF7);
        b->emit_cmp(2, 3);
        size_t sub_smi_bail_a = b->emit_branch_cond(b->cond_ne(), 0);
        b->emit_lsr_imm(2, R1, 48);
        b->emit_cmp(2, 3);
        size_t sub_smi_bail_b = b->emit_branch_cond(b->cond_ne(), 0);
        b->emit_lsl(4, R0, 16); b->emit_asr_imm(4, 4, 16);
        b->emit_lsl(5, R1, 16); b->emit_asr_imm(5, 5, 16);
        b->emit_sub(4, 4, 5);
        b->emit_lsl(4, 4, 16); b->emit_lsr(4, 4, 16);
        b->emit_load_imm64(3, Value::SMALLINT_TAG);
        b->emit_orr(4, 4, 3);
        b->emit_store_int(4, b->reg_base(), slot_offset(a));
        size_t sub_smi_done = b->emit_branch(0);
        b->patch_branch(sub_smi_bail_a, b->code_size());
        b->patch_branch(sub_smi_bail_b, b->code_size());
        b->emit_fmov_from_int(D0, R0);
        b->emit_fmov_from_int(D1, R1);
        b->emit_fsub(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        b->patch_branch(sub_smi_done, b->code_size());
        break;
    }
    case Opcode::MUL_NUM: {
        invalidate_fp_cache();
        // Load operands as raw NaN-boxed bits
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        if (b8 != c8) {
            b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        } else {
            b->emit_mov(R1, R0);
        }
        // Small int guard: upper 16 bits must == 0xFFF7
        b->emit_lsr_imm(2, R0, 48);
        b->emit_load_imm64(3, 0xFFF7);
        b->emit_cmp(2, 3);
        size_t mul_smi_bail_a = b->emit_branch_cond(b->cond_ne(), 0);
        b->emit_lsr_imm(2, R1, 48);
        b->emit_cmp(2, 3);
        size_t mul_smi_bail_b = b->emit_branch_cond(b->cond_ne(), 0);
        // Both small ints: extract signed 48-bit, multiply, re-tag
        b->emit_lsl(4, R0, 16); b->emit_asr_imm(4, 4, 16);  // sign-extend
        b->emit_lsl(5, R1, 16); b->emit_asr_imm(5, 5, 16);
        b->emit_mul(4, 4, 5);                                  // X4 = a * b
        b->emit_lsl(4, 4, 16); b->emit_lsr(4, 4, 16);        // mask 48 bits
        b->emit_load_imm64(3, Value::SMALLINT_TAG);
        b->emit_orr(4, 4, 3);                                  // re-tag
        b->emit_store_int(4, b->reg_base(), slot_offset(a));
        size_t mul_smi_done = b->emit_branch(0);
        // FP fallback
        b->patch_branch(mul_smi_bail_a, b->code_size());
        b->patch_branch(mul_smi_bail_b, b->code_size());
        b->emit_fmov_from_int(D0, R0);
        b->emit_fmov_from_int(D1, R1);
        b->emit_fmul(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        fp_cache_ = {true, slot_offset(a), D0};
        b->patch_branch(mul_smi_done, b->code_size());
        break;
    }
    case Opcode::DIV_NUM: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8));
        b->emit_fdiv(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::MOD_NUM: {
        // Inline fmod using trunc-based formula: fmod(a,b) = a - trunc(a/b)*b
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8));
        b->emit_fdiv(D2, D0, D1);
        b->emit_frintz(D2, D2);
        b->emit_fmul(D2, D2, D1);
        b->emit_fsub(D0, D0, D2);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::ADD_IMM: {
        // Small int fast path for x += small_constant
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        // Small int guard
        b->emit_lsr_imm(2, R0, 48);
        b->emit_load_imm64(3, 0xFFF7);
        b->emit_cmp(2, 3);
        size_t addi_smi_bail = b->emit_branch_cond(b->cond_ne(), 0);
        // Extract, add immediate, re-tag
        b->emit_lsl(4, R0, 16); b->emit_asr_imm(4, 4, 16);  // X4 = signed value
        b->emit_load_imm64(3, c8);                              // X3 = immediate
        b->emit_add(4, 4, 3);                                    // X4 = value + imm
        b->emit_lsl(4, 4, 16); b->emit_lsr(4, 4, 16);          // mask 48 bits
        b->emit_load_imm64(3, Value::SMALLINT_TAG);
        b->emit_orr(4, 4, 3);                                    // re-tag
        b->emit_store_int(4, b->reg_base(), slot_offset(a));
        size_t addi_smi_done = b->emit_branch(0);
        // FP fallback
        b->patch_branch(addi_smi_bail, b->code_size());
        b->emit_fmov_from_int(D0, R0);
        double imm_d = static_cast<double>(c8);
        uint64_t imm_bits;
        std::memcpy(&imm_bits, &imm_d, 8);
        b->emit_load_imm64(R0, imm_bits);
        b->emit_fmov_from_int(D1, R0);
        b->emit_fadd(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        b->patch_branch(addi_smi_done, b->code_size());
        break;
    }
    case Opcode::ADD_STR: {
        // String concatenation helper
        static auto jit_add_str = [](int64_t a, int64_t b) -> int64_t {
            Value va = bits_to_value(a);
            Value vb = bits_to_value(b);
            if (va.is_string() && vb.is_string()) {
                auto* result = get_string_table().intern(
                    va.as_string()->value + vb.as_string()->value);
                return value_to_bits(Value(static_cast<Obj*>(result)));
            }
            return NIL_BITS;
        };
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        emit_helper_call(reinterpret_cast<void*>(+jit_add_str));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::MOD_EQ_ZERO: {
        // Small int fast path: SDIV + MSUB instead of FP fmod chain
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8)); // n
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8)); // i
        // Small int guard
        b->emit_lsr_imm(2, R0, 48);
        b->emit_load_imm64(3, 0xFFF7);
        b->emit_cmp(2, 3);
        size_t mod_smi_bail_a = b->emit_branch_cond(b->cond_ne(), 0);
        b->emit_lsr_imm(2, R1, 48);
        b->emit_cmp(2, 3);
        size_t mod_smi_bail_b = b->emit_branch_cond(b->cond_ne(), 0);
        // Extract signed 48-bit values
        b->emit_lsl(4, R0, 16); b->emit_asr_imm(4, 4, 16);  // X4 = n
        b->emit_lsl(5, R1, 16); b->emit_asr_imm(5, 5, 16);  // X5 = i
        // n % i = n - (n/i)*i
        b->emit_sdiv(2, 4, 5);       // X2 = n / i
        b->emit_msub(2, 2, 5, 4);    // X2 = n - (n/i)*i
        // Check if remainder == 0
        b->emit_cmp_imm(2, 0);
        size_t mod_smi_is_zero = b->emit_branch_cond(b->cond_eq(), 0);
        // Not zero → FALSE
        b->emit_load_imm64(R0, static_cast<uint64_t>(FALSE_BITS));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        size_t mod_smi_done = b->emit_branch(0);
        // Zero → TRUE
        b->patch_branch(mod_smi_is_zero, b->code_size());
        b->emit_load_imm64(R0, static_cast<uint64_t>(TRUE_BITS));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        size_t mod_smi_done2 = b->emit_branch(0);
        // FP fallback: inline fmod(n, i) == 0
        b->patch_branch(mod_smi_bail_a, b->code_size());
        b->patch_branch(mod_smi_bail_b, b->code_size());
        b->emit_fmov_from_int(D0, R0);
        b->emit_fmov_from_int(D1, R1);
        b->emit_fdiv(D2, D0, D1);
        b->emit_frintz(D2, D2);
        b->emit_fmul(D2, D2, D1);
        b->emit_fsub(D2, D0, D2);
        b->emit_load_imm64(R0, 0);
        b->emit_fmov_from_int(D3, R0);
        b->emit_fcmp(D2, D3);
        size_t mod_zero_br = b->emit_branch_cond(b->cond_eq(), 0);
        b->emit_load_imm64(R0, static_cast<uint64_t>(FALSE_BITS));
        size_t mod_skip = b->emit_branch(0);
        size_t mod_true_pos = b->code_size();
        b->emit_load_imm64(R0, static_cast<uint64_t>(TRUE_BITS));
        size_t mod_store_pos = b->code_size();
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        b->patch_branch(mod_zero_br, mod_true_pos);
        b->patch_branch(mod_skip, mod_store_pos);
        b->patch_branch(mod_smi_done, b->code_size());
        b->patch_branch(mod_smi_done2, b->code_size());
        break;
    }

    // ================================================================
    // GENERIC ARITHMETIC — type-checked fast path for numbers
    // ================================================================

    // Helper macro: load both operands, move to FP, check both are numbers.
    // On success, D0/D1 hold the FP values and execution falls through.
    // On failure, branches to bail_target (defined by EMIT_BAIL_PATH).
    #define EMIT_NUM_GUARD(b8v, c8v) do { \
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8v)); \
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8v)); \
        b->emit_fmov_from_int(D0, R0); \
        b->emit_fmov_from_int(D1, R1); \
        b->emit_lsr_imm(R0, R0, 51); \
        b->emit_cmp_imm(R0, 0xFFF); \
        _bail0 = b->emit_branch_cond(b->cond_eq(), 0); \
        b->emit_lsr_imm(R1, R1, 51); \
        b->emit_cmp_imm(R1, 0xFFF); \
        _bail1 = b->emit_branch_cond(b->cond_eq(), 0); \
    } while(0)

    #define EMIT_BAIL_PATH(sp) do { \
        _done_br = b->emit_branch(0); \
        _bail_pos = b->code_size(); \
        emit_bailout(sp); \
        b->patch_branch(_bail0, _bail_pos); \
        b->patch_branch(_bail1, _bail_pos); \
        b->patch_branch(_done_br, b->code_size()); \
    } while(0)

    case Opcode::ADD: {
        size_t _bail0, _bail1, _done_br, _bail_pos;
        EMIT_NUM_GUARD(b8, c8);
        b->emit_fadd(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        EMIT_BAIL_PATH(start_pc);
        break;
    }
    case Opcode::SUB: {
        size_t _bail0, _bail1, _done_br, _bail_pos;
        EMIT_NUM_GUARD(b8, c8);
        b->emit_fsub(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        EMIT_BAIL_PATH(start_pc);
        break;
    }
    case Opcode::MUL: {
        size_t _bail0, _bail1, _done_br, _bail_pos;
        EMIT_NUM_GUARD(b8, c8);
        b->emit_fmul(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        EMIT_BAIL_PATH(start_pc);
        break;
    }
    case Opcode::DIV: {
        size_t _bail0, _bail1, _done_br, _bail_pos;
        EMIT_NUM_GUARD(b8, c8);
        b->emit_fdiv(D0, D0, D1);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        EMIT_BAIL_PATH(start_pc);
        break;
    }
    case Opcode::MOD: {
        size_t _bail0, _bail1, _done_br, _bail_pos;
        EMIT_NUM_GUARD(b8, c8);
        // fmod needs a helper call, but D0/D1 already hold the doubles
        emit_helper_call(reinterpret_cast<void*>(&jit_fmod));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        EMIT_BAIL_PATH(start_pc);
        break;
    }

    #undef EMIT_NUM_GUARD
    #undef EMIT_BAIL_PATH

    // ================================================================
    // COMPARISONS (type-specialized)
    // ================================================================

    #define EMIT_CMP_BRANCH_STORE(CC) do { \
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8)); \
        b->emit_load_fp(D1, b->reg_base(), slot_offset(c8)); \
        b->emit_fcmp(D0, D1); \
        size_t br = b->emit_branch_cond(b->CC(), 0); \
        b->emit_load_imm64(R0, static_cast<uint64_t>(FALSE_BITS)); \
        size_t skip = b->emit_branch(0); \
        size_t true_pos = b->code_size(); \
        b->emit_load_imm64(R0, static_cast<uint64_t>(TRUE_BITS)); \
        size_t store_pos = b->code_size(); \
        b->emit_store_int(R0, b->reg_base(), slot_offset(a)); \
        b->patch_branch(br, true_pos); \
        b->patch_branch(skip, store_pos); \
    } while(0)

    case Opcode::LT_NUM:  EMIT_CMP_BRANCH_STORE(cond_lt); break;
    case Opcode::LTE_NUM: EMIT_CMP_BRANCH_STORE(cond_le); break;
    case Opcode::GT_NUM:  EMIT_CMP_BRANCH_STORE(cond_gt); break;
    case Opcode::GTE_NUM: EMIT_CMP_BRANCH_STORE(cond_ge); break;
    case Opcode::EQ_NUM:  EMIT_CMP_BRANCH_STORE(cond_eq); break;
    case Opcode::NEQ_NUM: EMIT_CMP_BRANCH_STORE(cond_ne); break;

    #undef EMIT_CMP_BRANCH_STORE

    // ================================================================
    // GENERIC COMPARISONS — type-checked fast path for numbers
    // ================================================================

    #define EMIT_GENERIC_CMP(CC) do { \
        size_t _bail0, _bail1, _done_br, _bail_pos; \
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8)); \
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8)); \
        b->emit_fmov_from_int(D0, R0); \
        b->emit_fmov_from_int(D1, R1); \
        b->emit_lsr_imm(R0, R0, 51); \
        b->emit_cmp_imm(R0, 0xFFF); \
        _bail0 = b->emit_branch_cond(b->cond_eq(), 0); \
        b->emit_lsr_imm(R1, R1, 51); \
        b->emit_cmp_imm(R1, 0xFFF); \
        _bail1 = b->emit_branch_cond(b->cond_eq(), 0); \
        b->emit_fcmp(D0, D1); \
        size_t _br = b->emit_branch_cond(b->CC(), 0); \
        b->emit_load_imm64(R0, static_cast<uint64_t>(FALSE_BITS)); \
        size_t _skip = b->emit_branch(0); \
        size_t _true_pos = b->code_size(); \
        b->emit_load_imm64(R0, static_cast<uint64_t>(TRUE_BITS)); \
        size_t _store_pos = b->code_size(); \
        b->emit_store_int(R0, b->reg_base(), slot_offset(a)); \
        b->patch_branch(_br, _true_pos); \
        b->patch_branch(_skip, _store_pos); \
        _done_br = b->emit_branch(0); \
        _bail_pos = b->code_size(); \
        emit_bailout(start_pc); \
        b->patch_branch(_bail0, _bail_pos); \
        b->patch_branch(_bail1, _bail_pos); \
        b->patch_branch(_done_br, b->code_size()); \
    } while(0)

    case Opcode::EQ:  EMIT_GENERIC_CMP(cond_eq); break;
    case Opcode::NEQ: EMIT_GENERIC_CMP(cond_ne); break;
    case Opcode::LT:  EMIT_GENERIC_CMP(cond_lt); break;
    case Opcode::LTE: EMIT_GENERIC_CMP(cond_le); break;
    case Opcode::GT:  EMIT_GENERIC_CMP(cond_gt); break;
    case Opcode::GTE: EMIT_GENERIC_CMP(cond_ge); break;

    #undef EMIT_GENERIC_CMP

    // ================================================================
    // UNARY
    // ================================================================

    case Opcode::NEG: {
        b->emit_load_fp(D0, b->reg_base(), slot_offset(b8));
        b->emit_fneg(D0, D0);
        b->emit_store_fp(D0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::NOT: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_is_truthy));
        // Result is TRUE_BITS or FALSE_BITS. Negate by comparing.
        b->emit_load_imm64(R1, static_cast<uint64_t>(TRUE_BITS));
        b->emit_cmp(R0, R1);
        size_t was_true = b->emit_branch_cond(b->cond_eq(), 0);
        b->emit_load_imm64(R0, static_cast<uint64_t>(TRUE_BITS));
        size_t skip = b->emit_branch(0);
        size_t false_pos = b->code_size();
        b->emit_load_imm64(R0, static_cast<uint64_t>(FALSE_BITS));
        size_t end_pos = b->code_size();
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        b->patch_branch(was_true, false_pos);
        b->patch_branch(skip, end_pos);
        break;
    }

    // ================================================================
    // CONTROL FLOW
    // ================================================================

    case Opcode::JMP: {
        // NOTE: JMP offset is relative to the JMP instruction itself (not next instruction)
        // The interpreter does: ip += offset (where ip points to JMP)
        // So target = start_pc + sbx (not pc + sbx, since pc is already past JMP)
        int target_bc = start_pc + sbx;
        if (sbx < 0) {
            // Backward jump: target already compiled
            if (target_bc >= 0 && target_bc / 4 < (int)bc_to_code_.size()) {
                int target_code = bc_to_code_[target_bc / 4];
                if (target_code >= 0) {
                    size_t br = b->emit_branch(0);
                    b->patch_branch(br, static_cast<size_t>(target_code));
                } else {
                    emit_bailout(start_pc);
                    return false;
                }
            } else {
                emit_bailout(start_pc);
                return false;
            }
        } else if (sbx > 0) {
            // Forward jump: add fixup
            size_t br = b->emit_branch(0);
            fixups_.push_back({br, target_bc, 0});
        }
        // sbx == 0: no-op jump, skip
        break;
    }

    case Opcode::JMP_IF_FALSE: {
        int target_bc = pc + sbx;
        // Load value, call is_truthy, compare with TRUE_BITS
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        emit_helper_call(reinterpret_cast<void*>(&jit_is_truthy));
        b->emit_load_imm64(R1, static_cast<uint64_t>(TRUE_BITS));
        b->emit_cmp(R0, R1);
        // If NOT truthy (not equal to TRUE), branch to target
        size_t br = b->emit_branch_cond(b->cond_ne(), 0);
        if (sbx < 0) {
            if (target_bc >= 0 && target_bc / 4 < (int)bc_to_code_.size()) {
                int target_code = bc_to_code_[target_bc / 4];
                if (target_code >= 0) {
                    b->patch_branch(br, static_cast<size_t>(target_code));
                } else {
                    emit_bailout(start_pc);
                    return false;
                }
            } else {
                emit_bailout(start_pc);
                return false;
            }
        } else if (sbx > 0) {
            fixups_.push_back({br, target_bc, 1});
        }
        break;
    }

    case Opcode::JMP_IF_TRUE: {
        int target_bc = pc + sbx;
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        emit_helper_call(reinterpret_cast<void*>(&jit_is_truthy));
        b->emit_load_imm64(R1, static_cast<uint64_t>(TRUE_BITS));
        b->emit_cmp(R0, R1);
        // If truthy (equal to TRUE), branch to target
        size_t br = b->emit_branch_cond(b->cond_eq(), 0);
        if (sbx < 0) {
            if (target_bc >= 0 && target_bc / 4 < (int)bc_to_code_.size()) {
                int target_code = bc_to_code_[target_bc / 4];
                if (target_code >= 0) {
                    b->patch_branch(br, static_cast<size_t>(target_code));
                } else {
                    emit_bailout(start_pc);
                    return false;
                }
            } else {
                emit_bailout(start_pc);
                return false;
            }
        } else if (sbx > 0) {
            fixups_.push_back({br, target_bc, 1});
        }
        break;
    }

    // ================================================================
    // FUSED COMPARE-BRANCH
    // ================================================================

    #define EMIT_FUSED_CMP_BRANCH(COND_FN, CMP_OP) do { \
        int target_bc = pc + (int8_t)c8; \
        /* Small int fast path */ \
        b->emit_load_int(R0, b->reg_base(), slot_offset(a)); \
        b->emit_load_int(R1, b->reg_base(), slot_offset(b8)); \
        b->emit_lsr_imm(2, R0, 48); \
        b->emit_load_imm64(3, 0xFFF7); \
        b->emit_cmp(2, 3); \
        size_t _smi_bail_a = b->emit_branch_cond(b->cond_ne(), 0); \
        b->emit_lsr_imm(2, R1, 48); \
        b->emit_cmp(2, 3); \
        size_t _smi_bail_b = b->emit_branch_cond(b->cond_ne(), 0); \
        b->emit_lsl(R0, R0, 16); b->emit_asr_imm(R0, R0, 16); \
        b->emit_lsl(R1, R1, 16); b->emit_asr_imm(R1, R1, 16); \
        b->emit_cmp(R0, R1); \
        size_t _smi_br = b->emit_branch_cond(b->COND_FN(), 0); \
        size_t _smi_skip = b->emit_branch(0); \
        /* FP fallback */ \
        b->patch_branch(_smi_bail_a, b->code_size()); \
        b->patch_branch(_smi_bail_b, b->code_size()); \
        if (fp_cache_.valid && fp_cache_.slot_byte_offset == slot_offset(a) && fp_cache_.fp_reg == D0) { \
        } else { \
            b->emit_load_fp(D0, b->reg_base(), slot_offset(a)); \
        } \
        invalidate_fp_cache(); \
        b->emit_load_fp(D1, b->reg_base(), slot_offset(b8)); \
        b->emit_fcmp(D0, D1); \
        size_t _fp_br = b->emit_branch_cond(b->COND_FN(), 0); \
        /* Both no-branch paths converge */ \
        b->patch_branch(_smi_skip, b->code_size()); \
        /* Patch branch targets */ \
        if ((int8_t)c8 < 0) { \
            if (target_bc >= 0 && target_bc / 4 < (int)bc_to_code_.size()) { \
                int tc = bc_to_code_[target_bc / 4]; \
                if (tc >= 0) { \
                    b->patch_branch(_smi_br, (size_t)tc); \
                    b->patch_branch(_fp_br, (size_t)tc); \
                } else { emit_bailout(start_pc); return false; } \
            } else { emit_bailout(start_pc); return false; } \
        } else if ((int8_t)c8 > 0) { \
            fixups_.push_back({_smi_br, target_bc, 1}); \
            fixups_.push_back({_fp_br, target_bc, 1}); \
        } \
    } while(0)

    // JMP_IF_NOT_LT: branch if NOT less => branch if GE
    case Opcode::JMP_IF_NOT_LT:  EMIT_FUSED_CMP_BRANCH(cond_ge, ge); break;
    // JMP_IF_NOT_LTE: branch if NOT less-or-equal => branch if GT
    case Opcode::JMP_IF_NOT_LTE: EMIT_FUSED_CMP_BRANCH(cond_gt, gt); break;
    // JMP_IF_NOT_GT: branch if NOT greater => branch if LE
    case Opcode::JMP_IF_NOT_GT:  EMIT_FUSED_CMP_BRANCH(cond_le, le); break;
    // JMP_IF_NOT_GTE: branch if NOT greater-or-equal => branch if LT
    case Opcode::JMP_IF_NOT_GTE: EMIT_FUSED_CMP_BRANCH(cond_lt, lt); break;
    // JMP_IF_NOT_EQ: branch if NOT equal => branch if NE
    case Opcode::JMP_IF_NOT_EQ:  EMIT_FUSED_CMP_BRANCH(cond_ne, ne); break;

    #undef EMIT_FUSED_CMP_BRANCH

    // ================================================================
    // BITWISE OPERATORS
    // ================================================================

    case Opcode::BIT_AND: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        emit_helper_call(reinterpret_cast<void*>(&jit_bit_and));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::BIT_OR: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        emit_helper_call(reinterpret_cast<void*>(&jit_bit_or));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::BIT_XOR: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        emit_helper_call(reinterpret_cast<void*>(&jit_bit_xor));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::BIT_NOT: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_bit_not));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::SHL: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        emit_helper_call(reinterpret_cast<void*>(&jit_shl));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::SHR: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        emit_helper_call(reinterpret_cast<void*>(&jit_shr));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    // ================================================================
    // FUNCTION CALLS
    // ================================================================

    case Opcode::CALL: {
        // jit_call_helper(callee_slot, arg_count) → result bits in X0
        // callee_slot = &stack[base + a] = REG_BASE + a*8
        b->emit_load_imm64(R0, static_cast<uint64_t>(a * 8));
        b->emit_add(R0, b->reg_base(), R0); // X0 = &stack[base + a]
        b->emit_load_imm64(R1, static_cast<uint64_t>(c8)); // arg_count = C
        emit_helper_call(reinterpret_cast<void*>(&jit_call_helper));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    case Opcode::TAIL_CALL: {
        // Bail on tail calls for now
        emit_bailout(start_pc);
        return false;
    }

    // ================================================================
    // CLOSURE / UPVALUE CLOSE
    // ================================================================

    case Opcode::CLOSURE: {
        // Complex: needs inline upvalue descriptors in bytecode
        emit_bailout(start_pc);
        return false;
    }
    case Opcode::CLOSE_UPVALUE: {
        emit_bailout(start_pc);
        return false;
    }

    // ================================================================
    // RETURN
    // ================================================================

    case Opcode::RETURN: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        // Compute &stack_[callee_pos] = R_STACK + callee_pos * 8
        b->emit_add(R1, b->reg_callee(), b->reg_callee()); // ×2
        b->emit_add(R1, R1, R1);     // ×4
        b->emit_add(R1, R1, R1);     // ×8
        b->emit_add(R1, R1, b->reg_stack()); // + &stack_[0]
        b->emit_store_int(R0, R1, 0);
        b->emit_set_return(0);
        b->emit_epilogue();
        break;
    }

    // ================================================================
    // DATA STRUCTURES
    // ================================================================

    case Opcode::NEW_ARRAY: {
        // jit_new_array(elements_ptr, count) → bits
        // elements are at stack[base + a + 1] .. stack[base + a + b]
        b->emit_load_imm64(R0, static_cast<uint64_t>((a + 1) * 8));
        b->emit_add(R0, b->reg_base(), R0); // X0 = &stack[base + a + 1]
        b->emit_load_imm64(R1, static_cast<uint64_t>(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_new_array));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::NEW_MAP: {
        emit_helper_call(reinterpret_cast<void*>(&jit_new_map));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::GET_INDEX: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        emit_helper_call(reinterpret_cast<void*>(&jit_get_index));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::SET_INDEX: {
        // jit_set_index(obj, idx, val)
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        b->emit_load_int(R1, b->reg_base(), slot_offset(b8));
        b->emit_load_int(2, b->reg_base(), slot_offset(c8)); // X2 = value
        emit_helper_call(reinterpret_cast<void*>(&jit_set_index));
        break;
    }

    // ================================================================
    // FIELD ACCESS
    // ================================================================

    case Opcode::GET_FIELD: {
        // Bail on GET_FIELD - field access can be fragile in JIT with GC
        emit_bailout(start_pc);
        return false;
    }
    case Opcode::SET_FIELD: {
        // Bail on SET_FIELD - method field writes can be fragile in JIT
        emit_bailout(start_pc);
        return false;
    }

    // ================================================================
    // CLASS / OBJECT
    // ================================================================

    case Opcode::NEW_CLASS: {
        b->emit_load_int(R0, b->reg_const(), bx * 8); // X0 = name ObjString*
        emit_helper_call(reinterpret_cast<void*>(&jit_new_class));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::NEW_INSTANCE: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_new_instance));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::GET_METHOD: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_const(), c8 * 8);
        emit_helper_call(reinterpret_cast<void*>(&jit_get_method));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::INVOKE: {
        // Complex: combined method lookup + call
        emit_bailout(start_pc);
        return false;
    }

    // ================================================================
    // RANGE / ITERATOR
    // ================================================================

    case Opcode::NEW_RANGE: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_base(), slot_offset(c8));
        emit_helper_call(reinterpret_cast<void*>(&jit_new_range));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::ITER_INIT: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_iter_init));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::ITER_NEXT: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_iter_next));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::ITER_DONE: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_iter_done));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    // ================================================================
    // SPECIAL
    // ================================================================

    case Opcode::PRINT: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        emit_helper_call(reinterpret_cast<void*>(&jit_print));
        break;
    }
    case Opcode::HALT: {
        b->emit_set_return(0);
        b->emit_epilogue();
        break;
    }
    case Opcode::NOP:
        break;

    // ================================================================
    // FIBER / COROUTINE — bail
    // ================================================================

    case Opcode::FIBER_YIELD:
    case Opcode::FIBER_RESUME:
        emit_bailout(start_pc);
        return false;

    // ================================================================
    // AWAIT / EXCEPTION — bail
    // ================================================================

    case Opcode::AWAIT:
    case Opcode::THROW:
    case Opcode::TRY_BEGIN:
    case Opcode::TRY_END:
        emit_bailout(start_pc);
        return false;

    // ================================================================
    // WIDE PREFIX — bail
    // ================================================================

    case Opcode::WIDE:
        emit_bailout(start_pc);
        return false;

    // ================================================================
    // SIGNAL & EFFECT
    // ================================================================

    case Opcode::SIGNAL_CREATE: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        // Load signal name from a debug constant (or empty)
        b->emit_load_imm64(R1, 0); // NULL name
        emit_helper_call(reinterpret_cast<void*>(&jit_signal_create));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::SIGNAL_GET: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_signal_get));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::SIGNAL_SET: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        b->emit_load_int(R1, b->reg_base(), slot_offset(b8));
        emit_helper_call(reinterpret_cast<void*>(&jit_signal_set));
        break;
    }
    case Opcode::EFFECT_CREATE: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_imm64(R1, 0); // NULL name
        emit_helper_call(reinterpret_cast<void*>(&jit_effect_create));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::EFFECT_RUN: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        emit_helper_call(reinterpret_cast<void*>(&jit_effect_run));
        break;
    }

    // ================================================================
    // ENUM
    // ================================================================

    case Opcode::ENUM_CREATE: {
        b->emit_load_int(R0, b->reg_const(), bx * 8);
        emit_helper_call(reinterpret_cast<void*>(&jit_enum_create));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::ENUM_VARIANT: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        b->emit_load_int(R1, b->reg_const(), b8 * 8);
        b->emit_load_int(2, b->reg_base(), slot_offset(c8));
        emit_helper_call(reinterpret_cast<void*>(&jit_enum_variant));
        break;
    }
    case Opcode::ENUM_DATA_VARIANT: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(a));
        b->emit_load_int(R1, b->reg_const(), bx * 8);
        emit_helper_call(reinterpret_cast<void*>(&jit_enum_data_variant));
        break;
    }
    case Opcode::ENUM_GET: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_const(), c8 * 8);
        emit_helper_call(reinterpret_cast<void*>(&jit_enum_get));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }
    case Opcode::ENUM_IS: {
        b->emit_load_int(R0, b->reg_base(), slot_offset(b8));
        b->emit_load_int(R1, b->reg_const(), c8 * 8);
        emit_helper_call(reinterpret_cast<void*>(&jit_enum_is));
        b->emit_store_int(R0, b->reg_base(), slot_offset(a));
        break;
    }

    // ================================================================
    // DEFAULT: bail
    // ================================================================

    default:
        emit_bailout(start_pc);
        return false;
    }

    return true;
}

void JITCompiler::fixup_jumps() {
    for (auto& fixup : fixups_) {
        if (fixup.bc_target < 0 || fixup.bc_target / 4 >= static_cast<int>(bc_to_code_.size()))
            continue;
        int target_code = bc_to_code_[fixup.bc_target / 4];
        if (target_code < 0) continue;
        backend_->patch_branch(fixup.code_offset, static_cast<size_t>(target_code));
    }
    fixups_.clear();
}

JITCode* JITCompiler::compile(ObjFunction* function) {
    if (!function || function->bytecode.empty()) return nullptr;
    if (function->bytecode.size() < 16) return nullptr;
    if (!backend_) return nullptr;

    // Skip functions that capture upvalues (closures) — fragile in JIT
    if (!function->upvalue_descs.empty()) return nullptr;

    backend_->reset();
    fixups_.clear();
    invalidate_fp_cache();
    bytecode_ = function->bytecode;
    constants_ = function->constants;

    int total_bc = static_cast<int>(bytecode_.size());
    int total_inst = total_bc / 4;
    bc_to_code_.assign(total_inst + 1, -1);

    // Prologue
    backend_->emit_prologue();

    // Compile bytecode
    int pc = 0;
    while (pc < total_bc) {
        bc_to_code_[pc / 4] = static_cast<int>(backend_->code_size());
        compile_instruction(pc);
    }

    // End-of-function bailout
    emit_bailout(total_bc);

    // Fix up jumps
    fixup_jumps();

    // Finalize (allocate executable memory)
    return backend_->finalize();
}

// ============================================================
// JITCache
// ============================================================

JITCode* JITCache::get_or_compile(ObjFunction* func) {
    if (func->jit_code) return func->jit_code;
    JITCode* code = compiler.compile(func);
    if (code) {
        compiled[func] = code;
        func->jit_code = code;
    }
    return code;
}

void JITCache::record_call(ObjFunction* func) {
    call_counts[func]++;
}

JITCache::~JITCache() {
    for (auto& [func, code] : compiled) delete code;
    compiled.clear();
}

} // namespace akar
