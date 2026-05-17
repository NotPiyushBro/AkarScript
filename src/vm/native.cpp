#include "akar/vm/native.h"
#include "akar/vm/vm.h"
#include "akar/vm/object_file.h"
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <cstring>

namespace akar {

void register_native_function(VM& vm, const std::string& name, NativeFn fn) {
    vm.define_native(name, std::move(fn));
}

template<> Value to_value<double>(double val) { return Value(val); }
template<> Value to_value<int>(int val) { return Value(static_cast<double>(val)); }
template<> Value to_value<bool>(bool val) { return Value(val); }
template<> Value to_value<std::string>(std::string val) {
    return Value(static_cast<Obj*>(get_string_table().intern(std::move(val))));
}

double value_to_double(Value v) { return v.is_number() ? v.get_number() : 0; }
int value_to_int(Value v) { return v.is_number() ? static_cast<int>(v.get_number()) : 0; }
bool value_to_bool(Value v) { return v.is_truthy(); }
std::string value_to_string(Value v) { return v.to_string(); }

void register_builtins(VM& vm) {
    // print(...)
    vm.define_native("print", [](int argc, Value* argv) -> Value {
        for (int i = 0; i < argc; i++) {
            if (i > 0) std::cout << " ";
            std::cout << argv[i].to_string();
        }
        std::cout << std::endl;
        return Value();
    });

    // len(collection)
    vm.define_native("len", [](int argc, Value* argv) -> Value {
        if (argc != 1) return Value();
        if (argv[0].is_string()) return Value(static_cast<double>(argv[0].as_string()->value.size()));
        if (argv[0].is_array()) return Value(static_cast<double>(argv[0].as_array()->elements.size()));
        if (argv[0].is_map()) return Value(static_cast<double>(argv[0].as_map()->entries.size()));
        return Value(0.0);
    });

    // type(value)
    vm.define_native("type", [](int argc, Value* argv) -> Value {
        if (argc != 1) return Value();
        std::string t;
        if (argv[0].is_nil()) t = "nil";
        else if (argv[0].is_bool()) t = "bool";
        else if (argv[0].is_number()) t = "number";
        else if (argv[0].is_string()) t = "string";
        else if (argv[0].is_array()) t = "array";
        else if (argv[0].is_map()) t = "map";
        else if (argv[0].is_function() || argv[0].is_closure()) t = "function";
        else if (argv[0].is_class()) t = "class";
        else if (argv[0].is_instance()) t = "instance";
        else t = "unknown";
        return Value(static_cast<Obj*>(get_string_table().intern(t)));
    });

    // to_number(value)
    vm.define_native("to_number", [](int argc, Value* argv) -> Value {
        if (argc != 1) return Value();
        if (argv[0].is_number()) return argv[0];
        if (argv[0].is_string()) {
            try {
                const std::string& s = argv[0].as_string()->value;
                if (s.empty()) return Value(); // nil for empty string
                size_t pos;
                double val = std::stod(s, &pos);
                if (pos != s.size()) return Value(); // nil for partial parse ("12abc")
                return Value(val);
            } catch (...) { return Value(); }
        }
        if (argv[0].is_bool()) return Value(argv[0].get_bool() ? 1.0 : 0.0);
        return Value(); // nil for nil, arrays, maps, etc.
    });

    // to_string(value)
    vm.define_native("to_string", [](int argc, Value* argv) -> Value {
        if (argc != 1) return Value();
        return Value(static_cast<Obj*>(get_string_table().intern(argv[0].to_string())));
    });

    // push(array, value)
    vm.define_native("push", [](int argc, Value* argv) -> Value {
        if (argc != 2 || !argv[0].is_array()) return Value();
        argv[0].as_array()->elements.push_back(argv[1]);
        return argv[0];
    });

    // pop(array)
    vm.define_native("pop", [](int argc, Value* argv) -> Value {
        if (argc != 1 || !argv[0].is_array()) return Value();
        auto& elems = argv[0].as_array()->elements;
        if (elems.empty()) return Value();
        Value val = elems.back();
        elems.pop_back();
        return val;
    });

    // keys(map)
    vm.define_native("keys", [](int argc, Value* argv) -> Value {
        if (argc != 1 || !argv[0].is_map()) return Value();
        auto* arr = allocate_array();
        for (auto& [k, v] : argv[0].as_map()->entries) {
            arr->elements.push_back(Value(static_cast<Obj*>(get_string_table().intern(k))));
        }
        return Value(static_cast<Obj*>(arr));
    });

    // values(map)
    vm.define_native("values", [](int argc, Value* argv) -> Value {
        if (argc != 1 || !argv[0].is_map()) return Value();
        auto* arr = allocate_array();
        for (auto& [k, v] : argv[0].as_map()->entries) {
            arr->elements.push_back(v);
        }
        return Value(static_cast<Obj*>(arr));
    });

    // random()
    vm.define_native("random", [](int argc, Value*) -> Value {
        static bool seeded = false;
        if (!seeded) { std::srand(std::time(nullptr)); seeded = true; }
        return Value(static_cast<double>(std::rand()) / RAND_MAX);
    });

    // floor(x)
    vm.define_native("floor", [](int argc, Value* argv) -> Value {
        if (argc != 1 || !argv[0].is_number()) return Value();
        return Value(std::floor(argv[0].get_number()));
    });

    // ceil(x)
    vm.define_native("ceil", [](int argc, Value* argv) -> Value {
        if (argc != 1 || !argv[0].is_number()) return Value();
        return Value(std::ceil(argv[0].get_number()));
    });

    // abs(x)
    vm.define_native("abs", [](int argc, Value* argv) -> Value {
        if (argc != 1 || !argv[0].is_number()) return Value();
        return Value(std::abs(argv[0].get_number()));
    });

    // sqrt(x)
    vm.define_native("sqrt", [](int argc, Value* argv) -> Value {
        if (argc != 1 || !argv[0].is_number()) return Value();
        return Value(std::sqrt(argv[0].get_number()));
    });

    // clock() - returns CPU time in seconds
    vm.define_native("clock", [](int argc, Value*) -> Value {
        return Value(static_cast<double>(std::clock()) / CLOCKS_PER_SEC);
    });

    // time() - returns current Unix timestamp in seconds (with fractional precision)
    vm.define_native("time", [](int argc, Value*) -> Value {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        double seconds = std::chrono::duration<double>(duration).count();
        return Value(seconds);
    });

    // input(prompt) - reads a line from stdin, returns as string
    vm.define_native("input", [](int argc, Value* argv) -> Value {
        if (argc >= 1 && argv[0].is_string()) {
            std::cout << argv[0].as_string()->value;
            std::cout.flush();
        }
        std::string line;
        std::getline(std::cin, line);
        if (std::cin.eof()) return Value();
        return Value(static_cast<Obj*>(get_string_table().intern(line)));
    });

    // sleep(ms) - sleeps for given milliseconds
    vm.define_native("sleep", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_number()) return Value();
        int ms = static_cast<int>(argv[0].get_number());
        if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return Value();
    });

    // int(x) - truncate to integer
    vm.define_native("int", [](int argc, Value* argv) -> Value {
        if (argc < 1) return Value(0.0);
        if (argv[0].is_number()) return Value(static_cast<double>(static_cast<int64_t>(argv[0].get_number())));
        if (argv[0].is_string()) {
            try { return Value(static_cast<double>(std::stoll(argv[0].as_string()->value))); }
            catch (...) { return Value(0.0); }
        }
        if (argv[0].is_bool()) return Value(argv[0].get_bool() ? 1.0 : 0.0);
        return Value(0.0);
    });

    // char(code) - convert ASCII code to single-char string
    vm.define_native("char", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_number()) return Value();
        char c = static_cast<char>(static_cast<int>(argv[0].get_number()));
        std::string s(1, c);
        return Value(static_cast<Obj*>(get_string_table().intern(s)));
    });

    // ascii(str) - return ASCII code of first character
    vm.define_native("ascii", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        auto& s = argv[0].as_string()->value;
        if (s.empty()) return Value(0.0);
        return Value(static_cast<double>(static_cast<unsigned char>(s[0])));
    });

    // format(fmt, ...args) - basic string formatting: {} placeholders
    vm.define_native("format", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::string fmt = argv[0].as_string()->value;
        std::string result;
        size_t arg_idx = 1;
        size_t pos = 0;
        while (pos < fmt.size()) {
            if (fmt[pos] == '{' && pos + 1 < fmt.size() && fmt[pos + 1] == '{') {
                result += '{';
                pos += 2;
            } else if (fmt[pos] == '}' && pos + 1 < fmt.size() && fmt[pos + 1] == '}') {
                result += '}';
                pos += 2;
            } else if (fmt[pos] == '{' && pos + 1 < fmt.size() && fmt[pos + 1] == '}') {
                if (arg_idx < argc) {
                    result += argv[arg_idx].to_string();
                    arg_idx++;
                }
                pos += 2;
            } else {
                result += fmt[pos];
                pos++;
            }
        }
        return Value(static_cast<Obj*>(get_string_table().intern(result)));
    });

    // exit(code) - exit the program
    vm.define_native("exit", [](int argc, Value* argv) -> Value {
        int code = 0;
        if (argc >= 1 && argv[0].is_number()) code = static_cast<int>(argv[0].get_number());
        std::exit(code);
        return Value();
    });

    // assert(condition, message) - assertion
    vm.define_native("assert", [](int argc, Value* argv) -> Value {
        if (argc < 1) return Value();
        if (!argv[0].is_truthy()) {
            std::string msg = "Assertion failed";
            if (argc >= 2 && argv[1].is_string()) msg = argv[1].as_string()->value;
            std::cerr << "AssertionError: " << msg << std::endl;
            std::exit(1);
        }
        return Value();
    });

    // concat(str1, str2, ...) - concatenate strings (faster than + in loops)
    vm.define_native("concat", [](int argc, Value* argv) -> Value {
        std::string result;
        for (int i = 0; i < argc; i++) {
            result += argv[i].to_string();
        }
        return Value(static_cast<Obj*>(get_string_table().intern(result)));
    });

    // contains(collection, key) - check if array/string/map contains value/key
    vm.define_native("contains", [](int argc, Value* argv) -> Value {
        if (argc < 2) return Value(false);
        if (argv[0].is_string() && argv[1].is_string()) {
            return Value(argv[0].as_string()->value.find(argv[1].as_string()->value) != std::string::npos);
        }
        if (argv[0].is_array()) {
            for (auto& elem : argv[0].as_array()->elements) {
                if (elem == argv[1]) return Value(true);
            }
            return Value(false);
        }
        if (argv[0].is_map() && argv[1].is_string()) {
            return Value(argv[0].as_map()->entries.find(argv[1].as_string()->value) != argv[0].as_map()->entries.end());
        }
        return Value(false);
    });

    // replace(str, old, new) - string replacement
    vm.define_native("replace", [](int argc, Value* argv) -> Value {
        if (argc < 3 || !argv[0].is_string() || !argv[1].is_string() || !argv[2].is_string()) return Value();
        std::string str = argv[0].as_string()->value;
        std::string old_str = argv[1].as_string()->value;
        std::string new_str = argv[2].as_string()->value;
        if (old_str.empty()) return argv[0];
        size_t pos = 0;
        while ((pos = str.find(old_str, pos)) != std::string::npos) {
            str.replace(pos, old_str.size(), new_str);
            pos += new_str.size();
        }
        return Value(static_cast<Obj*>(get_string_table().intern(str)));
    });

    // split(str, delim) - split string into array
    vm.define_native("split", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_string()) return Value();
        auto* arr = allocate_array();
        std::string str = argv[0].as_string()->value;
        std::string delim = argv[1].as_string()->value;
        if (delim.empty()) {
            for (char c : str) {
                arr->elements.push_back(Value(static_cast<Obj*>(get_string_table().intern(std::string(1, c)))));
            }
            return Value(static_cast<Obj*>(arr));
        }
        size_t pos = 0;
        while (true) {
            size_t found = str.find(delim, pos);
            if (found == std::string::npos) {
                arr->elements.push_back(Value(static_cast<Obj*>(get_string_table().intern(str.substr(pos)))));
                break;
            }
            arr->elements.push_back(Value(static_cast<Obj*>(get_string_table().intern(str.substr(pos, found - pos)))));
            pos = found + delim.size();
        }
        return Value(static_cast<Obj*>(arr));
    });

    // join(array, delim) - join array of strings
    vm.define_native("join", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_string()) return Value();
        std::string delim = argv[1].as_string()->value;
        std::string result;
        auto& elems = argv[0].as_array()->elements;
        for (size_t i = 0; i < elems.size(); i++) {
            if (i > 0) result += delim;
            result += elems[i].to_string();
        }
        return Value(static_cast<Obj*>(get_string_table().intern(result)));
    });

    // substr(str, start, len) - substring
    vm.define_native("substr", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_number()) return Value();
        auto& str = argv[0].as_string()->value;
        int start = static_cast<int>(argv[1].get_number());
        int len = (argc >= 3 && argv[2].is_number()) ? static_cast<int>(argv[2].get_number()) : static_cast<int>(str.size()) - start;
        if (start < 0) start = 0;
        if (start >= static_cast<int>(str.size())) return Value(static_cast<Obj*>(get_string_table().intern("")));
        if (len < 0) len = 0;
        return Value(static_cast<Obj*>(get_string_table().intern(str.substr(start, len))));
    });

    // range(start, end) - creates array of numbers
    vm.define_native("range", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_number() || !argv[1].is_number()) return Value();
        auto* arr = allocate_array();
        int start = static_cast<int>(argv[0].get_number());
        int end = static_cast<int>(argv[1].get_number());
        int step = 1;
        if (argc >= 3 && argv[2].is_number()) {
            step = static_cast<int>(argv[2].get_number());
            if (step == 0) step = 1;
        }
        if (step > 0) {
            for (int i = start; i <= end; i += step) {
                arr->elements.push_back(Value(static_cast<double>(i)));
            }
        } else {
            for (int i = start; i >= end; i += step) {
                arr->elements.push_back(Value(static_cast<double>(i)));
            }
        }
        return Value(static_cast<Obj*>(arr));
    });

    // Fiber/coroutine support
    // fiber_create(fn) - creates a new fiber from a function
    vm.define_native("fiber_create", [](int argc, Value* argv) -> Value {
        if (argc < 1 || (!argv[0].is_closure() && !argv[0].is_function())) {
            return Value();
        }
        auto* fiber = allocate_fiber();
        if (argv[0].is_closure()) {
            fiber->entry = argv[0].as_closure();
        } else {
            fiber->entry = allocate_closure(argv[0].as_function());
        }
        // Store extra args for the first resume
        for (int i = 1; i < argc; i++) {
            fiber->initial_args.push_back(argv[i]);
        }
        return Value(static_cast<Obj*>(fiber));
    });

    // fiber_yield(value) - yields a value from the current fiber
    vm.define_native("fiber_yield", [&vm](int argc, Value* argv) -> Value {
        vm.yield_pending_ = true;
        vm.yield_value_ = (argc >= 1) ? argv[0] : Value();
        return vm.yield_value_;
    });

    // fiber_resume(fiber, value) - resumes a fiber with a value
    // Sets a deferred resume flag; the run loop handles the actual frame push
    vm.define_native("fiber_resume", [&vm](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_fiber()) return Value();
        auto* fiber = argv[0].as_fiber();
        bool has_resume_val = (argc >= 2);
        Value resume_val = has_resume_val ? argv[1] : Value();
        int arg_count = has_resume_val ? 1 : 0;
        if (fiber->state == ObjFiber::State::Done) return Value();
        // Set deferred resume - the run loop will handle the actual call
        vm.resume_pending_ = true;
        vm.resume_fiber_ = fiber;
        vm.resume_value_ = resume_val;
        vm.resume_has_value_ = has_resume_val;
        vm.resume_return_reg_ = 0; // will be set by the CALL handler
        vm.resume_arg_count_ = arg_count;
        return Value(); // placeholder, actual result comes from fiber
    });

    // fiber_status(fiber) - returns fiber state as string
    vm.define_native("fiber_status", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_fiber()) return Value();
        auto* fiber = argv[0].as_fiber();
        const char* s = "created";
        if (fiber->state == ObjFiber::State::Running) s = "running";
        else if (fiber->state == ObjFiber::State::Suspended) s = "suspended";
        else if (fiber->state == ObjFiber::State::Done) s = "done";
        return Value(static_cast<Obj*>(get_string_table().intern(s)));
    });
}

} // namespace akar
