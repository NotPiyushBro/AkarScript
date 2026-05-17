#include "akar/common/value.h"
#include <sstream>
#include <cmath>

namespace akar {

static Obj* g_objects = nullptr;
static StringTable g_string_table;
static size_t g_allocated_bytes = 0;
static size_t g_memory_limit = 256 * 1024 * 1024; // 256 MB default
static size_t g_next_gc = 1024;
static std::vector<Obj*> g_gray_stack;

static void track_alloc(size_t bytes) {
    g_allocated_bytes += bytes;
}

size_t get_allocated_bytes() { return g_allocated_bytes; }
void reset_allocated_bytes() { g_allocated_bytes = 0; }
size_t get_memory_limit() { return g_memory_limit; }
void set_memory_limit(size_t limit) { g_memory_limit = limit; }

bool memory_limit_exceeded() {
    return g_memory_limit > 0 && g_allocated_bytes >= g_memory_limit;
}

size_t get_next_gc() { return g_next_gc; }
void set_next_gc(size_t threshold) { g_next_gc = threshold; }

void free_all_objects() {
    Obj* obj = g_objects;
    while (obj) {
        Obj* next = obj->next;
        delete obj;
        obj = next;
    }
    g_objects = nullptr;
    g_allocated_bytes = 0;
    g_gray_stack.clear();
    // Also clear string table so it doesn't hold dangling pointers
    g_string_table = StringTable();
}

// --- Garbage collection: tri-color mark-sweep ---

void gc_mark_object(Obj* obj) {
    if (!obj || obj->marked) return;
    obj->marked = true;
    g_gray_stack.push_back(obj);
}

void gc_mark_value(Value val) {
    if (val.is_obj()) {
        gc_mark_object(val.as_obj());
    }
}

void gc_trace_references(Obj* obj) {
    switch (obj->type) {
        case ObjType::Array: {
            auto* arr = static_cast<ObjArray*>(obj);
            for (auto& elem : arr->elements) {
                gc_mark_value(elem);
            }
            break;
        }
        case ObjType::Map: {
            auto* map = static_cast<ObjMap*>(obj);
            for (auto& [key, val] : map->entries) {
                gc_mark_value(val);
            }
            break;
        }
        case ObjType::Function: {
            auto* func = static_cast<ObjFunction*>(obj);
            for (auto& c : func->constants) {
                gc_mark_value(c);
            }
            break;
        }
        case ObjType::Closure: {
            auto* closure = static_cast<ObjClosure*>(obj);
            gc_mark_object(static_cast<Obj*>(closure->function));
            for (auto* uv : closure->upvalues) {
                gc_mark_object(static_cast<Obj*>(uv));
            }
            break;
        }
        case ObjType::Class: {
            auto* klass = static_cast<ObjClass*>(obj);
            for (auto& [name, method] : klass->methods) {
                gc_mark_value(method);
            }
            break;
        }
        case ObjType::Instance: {
            auto* inst = static_cast<ObjInstance*>(obj);
            gc_mark_object(static_cast<Obj*>(inst->klass));
            for (auto& [name, val] : inst->fields) {
                gc_mark_value(val);
            }
            break;
        }
        case ObjType::Upvalue: {
            auto* uv = static_cast<ObjUpvalue*>(obj);
            gc_mark_value(uv->closed);
            break;
        }
        case ObjType::Fiber: {
            auto* fiber = static_cast<ObjFiber*>(obj);
            gc_mark_object(static_cast<Obj*>(fiber->entry));
            if (fiber->parent) gc_mark_object(static_cast<Obj*>(fiber->parent));
            gc_mark_value(fiber->yielded_value);
            gc_mark_value(fiber->resume_value);
            for (auto& v : fiber->initial_args) {
                gc_mark_value(v);
            }
            for (auto& v : fiber->saved_stack) {
                gc_mark_value(v);
            }
            for (ObjUpvalue* uv = fiber->saved_open_upvalues; uv; uv = uv->next_upvalue) {
                gc_mark_object(static_cast<Obj*>(uv));
            }
            break;
        }
        case ObjType::String:
        case ObjType::Native:
            break; // no child references
    }
}

size_t gc_drain_gray_stack() {
    size_t traced = 0;
    while (!g_gray_stack.empty()) {
        Obj* obj = g_gray_stack.back();
        g_gray_stack.pop_back();
        gc_trace_references(obj);
        traced++;
    }
    return traced;
}

void gc_sweep() {
    Obj** prev = &g_objects;
    Obj* obj = g_objects;
    while (obj) {
        if (obj->marked) {
            obj->marked = false; // reset for next cycle
            prev = &obj->next;
            obj = obj->next;
        } else {
            Obj* unreached = obj;
            obj = obj->next;
            *prev = obj;
            if (g_allocated_bytes >= sizeof(Obj)) {
                g_allocated_bytes -= sizeof(Obj);
            } else {
                g_allocated_bytes = 0;
            }
            delete unreached;
        }
    }
}

void gc_mark_string_table() {
    get_string_table().mark_all();
}

ObjString* allocate_string(std::string value) {
    size_t bytes = sizeof(ObjString) + value.capacity();
    track_alloc(bytes);
    auto* obj = new ObjString(std::move(value));
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjArray* allocate_array() {
    track_alloc(sizeof(ObjArray));
    auto* obj = new ObjArray();
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjMap* allocate_map() {
    track_alloc(sizeof(ObjMap));
    auto* obj = new ObjMap();
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjFunction* allocate_function() {
    track_alloc(sizeof(ObjFunction));
    auto* obj = new ObjFunction();
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjClosure* allocate_closure(ObjFunction* func) {
    track_alloc(sizeof(ObjClosure));
    auto* obj = new ObjClosure(func);
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjClass* allocate_class(std::string name) {
    size_t bytes = sizeof(ObjClass) + name.capacity();
    track_alloc(bytes);
    auto* obj = new ObjClass(std::move(name));
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjInstance* allocate_instance(ObjClass* klass) {
    track_alloc(sizeof(ObjInstance));
    auto* obj = new ObjInstance(klass);
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjNative* allocate_native(NativeFn fn, std::string name) {
    size_t bytes = sizeof(ObjNative) + name.capacity();
    track_alloc(bytes);
    auto* obj = new ObjNative(std::move(fn), std::move(name));
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjUpvalue* allocate_upvalue(Value* slot) {
    track_alloc(sizeof(ObjUpvalue));
    auto* obj = new ObjUpvalue(slot);
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjFiber* allocate_fiber() {
    track_alloc(sizeof(ObjFiber));
    auto* obj = new ObjFiber();
    obj->next = g_objects;
    g_objects = obj;
    return obj;
}

ObjString* StringTable::intern(const std::string& s) {
    auto it = strings_.find(s);
    if (it != strings_.end()) return it->second;
    auto* str = allocate_string(s);
    strings_[s] = str;
    return str;
}

StringTable& get_string_table() {
    return g_string_table;
}

void StringTable::mark_all() {
    for (auto& [s, ptr] : strings_) {
        gc_mark_object(static_cast<Obj*>(ptr));
    }
}

// Value methods - NaN-boxed implementation
bool Value::is_string() const { return is_obj() && as_obj()->type == ObjType::String; }
bool Value::is_array() const { return is_obj() && as_obj()->type == ObjType::Array; }
bool Value::is_map() const { return is_obj() && as_obj()->type == ObjType::Map; }
bool Value::is_function() const { return is_obj() && as_obj()->type == ObjType::Function; }
bool Value::is_closure() const { return is_obj() && as_obj()->type == ObjType::Closure; }
bool Value::is_class() const { return is_obj() && as_obj()->type == ObjType::Class; }
bool Value::is_instance() const { return is_obj() && as_obj()->type == ObjType::Instance; }
bool Value::is_native() const { return is_obj() && as_obj()->type == ObjType::Native; }
bool Value::is_fiber() const { return is_obj() && as_obj()->type == ObjType::Fiber; }

ObjString* Value::as_string() const { return static_cast<ObjString*>(as_obj()); }
ObjArray* Value::as_array() const { return static_cast<ObjArray*>(as_obj()); }
ObjMap* Value::as_map() const { return static_cast<ObjMap*>(as_obj()); }
ObjFunction* Value::as_function() const { return static_cast<ObjFunction*>(as_obj()); }
ObjClosure* Value::as_closure() const { return static_cast<ObjClosure*>(as_obj()); }
ObjClass* Value::as_class() const { return static_cast<ObjClass*>(as_obj()); }
ObjInstance* Value::as_instance() const { return static_cast<ObjInstance*>(as_obj()); }
ObjNative* Value::as_native() const { return static_cast<ObjNative*>(as_obj()); }
ObjFiber* Value::as_fiber() const { return static_cast<ObjFiber*>(as_obj()); }

bool Value::is_truthy() const {
    if (is_nil()) return false;
    if (is_bool()) return get_bool();
    if (is_number()) {
        double d;
        std::memcpy(&d, &bits, 8);
        return d != 0 && !std::isnan(d);
    }
    return true; // objects are truthy
}

bool Value::operator==(const Value& other) const {
    // Fast path: same bits means same value (covers nil, bool, same pointer)
    if (bits == other.bits) {
        // For numbers: +0 == -0 must be true, but they have different bits.
        // Since bits match here, it's either the exact same number or same non-number.
        return true;
    }
    // Both numbers: compare as doubles
    if (is_number() && other.is_number()) {
        double a, b;
        std::memcpy(&a, &bits, 8);
        std::memcpy(&b, &other.bits, 8);
        return a == b;
    }
    // Both strings: compare contents
    if (is_string() && other.is_string()) {
        return as_string()->value == other.as_string()->value;
    }
    // Different types or different pointers
    return false;
}

std::string Value::to_string() const {
    if (is_nil()) return "nil";
    if (is_bool()) return get_bool() ? "true" : "false";
    if (is_number()) {
        double d;
        std::memcpy(&d, &bits, 8);
        if (std::floor(d) == d && std::abs(d) < 1e15) {
            return std::to_string(static_cast<int64_t>(d));
        }
        std::ostringstream ss;
        ss << d;
        return ss.str();
    }
    if (is_obj()) {
        auto* obj = as_obj();
        switch (obj->type) {
            case ObjType::String: return as_string()->value;
            case ObjType::Array: {
                std::string result = "[";
                auto& elems = as_array()->elements;
                for (size_t i = 0; i < elems.size(); i++) {
                    if (i > 0) result += ", ";
                    result += elems[i].to_string();
                }
                result += "]";
                return result;
            }
            case ObjType::Map: {
                std::string result = "{";
                auto& entries = as_map()->entries;
                bool first = true;
                for (auto& [k, v] : entries) {
                    if (!first) result += ", ";
                    first = false;
                    result += k + ": " + v.to_string();
                }
                result += "}";
                return result;
            }
            case ObjType::Function: return "<fn " + as_function()->name + ">";
            case ObjType::Closure: return "<fn " + as_closure()->function->name + ">";
            case ObjType::Class: return "<class " + as_class()->name + ">";
            case ObjType::Instance: return "<" + as_instance()->klass->name + " instance>";
            case ObjType::Native: return "<native " + as_native()->name + ">";
            case ObjType::Fiber: {
                const char* state_str = "created";
                if (as_fiber()->state == ObjFiber::State::Running) state_str = "running";
                else if (as_fiber()->state == ObjFiber::State::Suspended) state_str = "suspended";
                else if (as_fiber()->state == ObjFiber::State::Done) state_str = "done";
                return std::string("<fiber ") + state_str + ">";
            }
            case ObjType::Upvalue: return "<upvalue>";
        }
    }
    return "<unknown>";
}

} // namespace akar
