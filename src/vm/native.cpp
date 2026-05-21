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
#include <random>
#include <limits>

namespace akar {

// Safe double-to-int conversion (clamps to avoid UB)
static int safe_int(double d) {
    if (d != d) return 0; // NaN
    if (d > 2147483647.0) return 2147483647;
    if (d < -2147483648.0) return -2147483648;
    return static_cast<int>(d);
}

static int64_t safe_int64(double d) {
    if (d != d) return 0; // NaN
    if (d > 9223372036854775807.0) return 9223372036854775807LL;
    if (d < -9223372036854775808.0) return -9223372036854775807LL - 1;
    return static_cast<int64_t>(d);
}

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
int value_to_int(Value v) { return v.is_number() ? safe_int(v.get_number()) : 0; }
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
        return Value(); // nil for unsupported types
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
    vm.define_native("random", [](int, Value*) -> Value {
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value(dist(rng));
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
    vm.define_native("clock", [](int, Value*) -> Value {
        return Value(static_cast<double>(std::clock()) / CLOCKS_PER_SEC);
    });

    // time() - returns current Unix timestamp in seconds (with fractional precision)
    vm.define_native("time", [](int, Value*) -> Value {
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
        int ms = safe_int(argv[0].get_number());
        if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return Value();
    });

    // int(x) - truncate to integer
    vm.define_native("int", [](int argc, Value* argv) -> Value {
        if (argc < 1) return Value(0.0);
        if (argv[0].is_number()) return Value(static_cast<double>(safe_int64(argv[0].get_number())));
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
        char c = static_cast<char>(safe_int(argv[0].get_number()));
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

    // signal_value(signal_obj) - read a signal's value without dependency tracking
    vm.define_native("signal_value", [](int argc, Value* argv) -> Value {
        if (argc != 1 || !argv[0].is_signal()) return Value();
        return argv[0].as_signal()->value;
    });

    // signal_set(signal_obj, value) - set a signal's value (triggers effects)
    // Note: This is a native helper; prefer using `signal_name = value` syntax.
    // This is mainly for programmatic signal manipulation from native code.

    // ---- Profiling & Tracing ----

    // profile_start() - enable profiling
    vm.define_native("profile_start", [&vm](int, Value*) -> Value {
        vm.profiler_.start_profiling();
        return Value();
    });

    // profile_stop() - disable profiling
    vm.define_native("profile_stop", [&vm](int, Value*) -> Value {
        vm.profiler_.stop_profiling();
        return Value();
    });

    // profile_report() - print profiling report to stderr
    vm.define_native("profile_report", [&vm](int, Value*) -> Value {
        vm.profiler_.print_profile_report();
        return Value();
    });

    // profile_reset() - reset all profiling data
    vm.define_native("profile_reset", [&vm](int, Value*) -> Value {
        vm.profiler_.reset();
        return Value();
    });

    // trace_start() - enable tracing (also enables profiling)
    vm.define_native("trace_start", [&vm](int, Value*) -> Value {
        vm.profiler_.start_tracing();
        return Value();
    });

    // trace_stop() - disable tracing
    vm.define_native("trace_stop", [&vm](int, Value*) -> Value {
        vm.profiler_.stop_tracing();
        return Value();
    });

    // trace_dump() - print trace log to stderr
    vm.define_native("trace_dump", [&vm](int, Value*) -> Value {
        vm.profiler_.print_trace_log();
        return Value();
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
                if (arg_idx < static_cast<size_t>(argc)) {
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
        if (argc >= 1 && argv[0].is_number()) code = safe_int(argv[0].get_number());
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
        int start = safe_int(argv[1].get_number());
        int len = (argc >= 3 && argv[2].is_number()) ? safe_int(argv[2].get_number()) : static_cast<int>(str.size()) - start;
        if (start < 0) start = 0;
        if (start >= static_cast<int>(str.size())) return Value(static_cast<Obj*>(get_string_table().intern("")));
        if (len < 0) len = 0;
        return Value(static_cast<Obj*>(get_string_table().intern(str.substr(start, len))));
    });

    // range(start, end) - creates array of numbers
    vm.define_native("range", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_number() || !argv[1].is_number()) return Value();
        auto* arr = allocate_array();
        int start = safe_int(argv[0].get_number());
        int end = safe_int(argv[1].get_number());
        int step = 1;
        if (argc >= 3 && argv[2].is_number()) {
            step = safe_int(argv[2].get_number());
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

    // ========== Standard Math Library ==========

    // ========== Standard Math Library ==========

    // --- Constants (set as globals, not functions) ---
    vm.set_global("PI", Value(M_PI));
    vm.set_global("E", Value(M_E));

    // Special values
    vm.define_native("nan", [](int, Value*) -> Value { return Value(std::numeric_limits<double>::quiet_NaN()); });
    vm.define_native("inf", [](int, Value*) -> Value { return Value(std::numeric_limits<double>::infinity()); });
    vm.define_native("isnan", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() && std::isnan(argv[0].get_number()); });
    vm.define_native("isinf", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() && std::isinf(argv[0].get_number()); });

    // Trig
    vm.define_native("sin", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::sin(argv[0].get_number())) : Value(); });
    vm.define_native("cos", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::cos(argv[0].get_number())) : Value(); });
    vm.define_native("tan", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::tan(argv[0].get_number())) : Value(); });
    vm.define_native("asin", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::asin(argv[0].get_number())) : Value(); });
    vm.define_native("acos", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::acos(argv[0].get_number())) : Value(); });
    vm.define_native("atan", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::atan(argv[0].get_number())) : Value(); });
    vm.define_native("atan2", [](int argc, Value* argv) -> Value { return argc >= 2 ? Value(std::atan2(argv[0].get_number(), argv[1].get_number())) : Value(); });

    // Hyperbolic
    vm.define_native("sinh", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::sinh(argv[0].get_number())) : Value(); });
    vm.define_native("cosh", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::cosh(argv[0].get_number())) : Value(); });
    vm.define_native("tanh", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::tanh(argv[0].get_number())) : Value(); });

    // Logarithmic / Exponential
    vm.define_native("log", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::log(argv[0].get_number())) : Value(); });
    vm.define_native("log2", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::log2(argv[0].get_number())) : Value(); });
    vm.define_native("log10", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::log10(argv[0].get_number())) : Value(); });
    vm.define_native("exp", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::exp(argv[0].get_number())) : Value(); });
    vm.define_native("pow", [](int argc, Value* argv) -> Value { return argc >= 2 ? Value(std::pow(argv[0].get_number(), argv[1].get_number())) : Value(); });

    // Rounding
    vm.define_native("round", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::round(argv[0].get_number())) : Value(); });
    vm.define_native("trunc", [](int argc, Value* argv) -> Value { return argc >= 1 && argv[0].is_number() ? Value(std::trunc(argv[0].get_number())) : Value(); });

    // Comparison (variadic)
    vm.define_native("min", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_number()) return Value();
        double m = argv[0].get_number();
        for (int i = 1; i < argc; i++) { if (argv[i].is_number() && argv[i].get_number() < m) m = argv[i].get_number(); }
        return Value(m);
    });
    vm.define_native("max", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_number()) return Value();
        double m = argv[0].get_number();
        for (int i = 1; i < argc; i++) { if (argv[i].is_number() && argv[i].get_number() > m) m = argv[i].get_number(); }
        return Value(m);
    });
    vm.define_native("clamp", [](int argc, Value* argv) -> Value {
        if (argc < 3) return Value();
        double val = argv[0].get_number(), lo = argv[1].get_number(), hi = argv[2].get_number();
        if (val < lo) return Value(lo); if (val > hi) return Value(hi); return Value(val);
    });

    // Game math
    vm.define_native("sign", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_number()) return Value(0.0);
        double x = argv[0].get_number(); return Value(x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0));
    });
    vm.define_native("lerp", [](int argc, Value* argv) -> Value {
        if (argc < 3) return Value();
        return Value(argv[0].get_number() + (argv[1].get_number() - argv[0].get_number()) * argv[2].get_number());
    });
    vm.define_native("deg_to_rad", [](int argc, Value* argv) -> Value { return argc >= 1 ? Value(argv[0].get_number() * M_PI / 180.0) : Value(); });
    vm.define_native("rad_to_deg", [](int argc, Value* argv) -> Value { return argc >= 1 ? Value(argv[0].get_number() * 180.0 / M_PI) : Value(); });
    vm.define_native("fmod", [](int argc, Value* argv) -> Value { return argc >= 2 ? Value(std::fmod(argv[0].get_number(), argv[1].get_number())) : Value(); });

    // Vec2 [x, y]
    vm.define_native("vec2", [](int argc, Value* argv) -> Value {
        auto* arr = allocate_array();
        arr->elements.push_back(argc >= 1 ? argv[0] : Value(0.0));
        arr->elements.push_back(argc >= 2 ? argv[1] : Value(0.0));
        return Value(static_cast<Obj*>(arr));
    });
    vm.define_native("vec2_add", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_array()) return Value();
        auto* a = argv[0].as_array(); auto* b = argv[1].as_array();
        auto* r = allocate_array();
        r->elements.push_back(Value(a->elements[0].get_number() + b->elements[0].get_number()));
        r->elements.push_back(Value(a->elements[1].get_number() + b->elements[1].get_number()));
        return Value(static_cast<Obj*>(r));
    });
    vm.define_native("vec2_sub", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_array()) return Value();
        auto* a = argv[0].as_array(); auto* b = argv[1].as_array();
        auto* r = allocate_array();
        r->elements.push_back(Value(a->elements[0].get_number() - b->elements[0].get_number()));
        r->elements.push_back(Value(a->elements[1].get_number() - b->elements[1].get_number()));
        return Value(static_cast<Obj*>(r));
    });
    vm.define_native("vec2_scale", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_number()) return Value();
        auto* v = argv[0].as_array(); double s = argv[1].get_number();
        auto* r = allocate_array();
        r->elements.push_back(Value(v->elements[0].get_number() * s));
        r->elements.push_back(Value(v->elements[1].get_number() * s));
        return Value(static_cast<Obj*>(r));
    });
    vm.define_native("vec2_dot", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_array()) return Value();
        auto* a = argv[0].as_array(); auto* b = argv[1].as_array();
        return Value(a->elements[0].get_number() * b->elements[0].get_number() + a->elements[1].get_number() * b->elements[1].get_number());
    });
    vm.define_native("vec2_len", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_array()) return Value();
        auto* v = argv[0].as_array();
        double x = v->elements[0].get_number(), y = v->elements[1].get_number();
        return Value(std::sqrt(x * x + y * y));
    });
    vm.define_native("vec2_normalize", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_array()) return Value();
        auto* v = argv[0].as_array();
        double x = v->elements[0].get_number(), y = v->elements[1].get_number();
        double len = std::sqrt(x * x + y * y);
        if (len == 0) { auto* r = allocate_array(); r->elements.push_back(Value(0.0)); r->elements.push_back(Value(0.0)); return Value(static_cast<Obj*>(r)); }
        auto* r = allocate_array(); r->elements.push_back(Value(x / len)); r->elements.push_back(Value(y / len));
        return Value(static_cast<Obj*>(r));
    });
    vm.define_native("vec2_dist", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_array()) return Value();
        auto* a = argv[0].as_array(); auto* b = argv[1].as_array();
        double dx = a->elements[0].get_number() - b->elements[0].get_number();
        double dy = a->elements[1].get_number() - b->elements[1].get_number();
        return Value(std::sqrt(dx * dx + dy * dy));
    });

    // Vec3 [x, y, z]
    vm.define_native("vec3", [](int argc, Value* argv) -> Value {
        auto* arr = allocate_array();
        arr->elements.push_back(argc >= 1 ? argv[0] : Value(0.0));
        arr->elements.push_back(argc >= 2 ? argv[1] : Value(0.0));
        arr->elements.push_back(argc >= 3 ? argv[2] : Value(0.0));
        return Value(static_cast<Obj*>(arr));
    });
    vm.define_native("vec3_add", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_array()) return Value();
        auto* a = argv[0].as_array(); auto* b = argv[1].as_array(); auto* r = allocate_array();
        for (int i = 0; i < 3; i++) r->elements.push_back(Value(a->elements[i].get_number() + b->elements[i].get_number()));
        return Value(static_cast<Obj*>(r));
    });
    vm.define_native("vec3_sub", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_array()) return Value();
        auto* a = argv[0].as_array(); auto* b = argv[1].as_array(); auto* r = allocate_array();
        for (int i = 0; i < 3; i++) r->elements.push_back(Value(a->elements[i].get_number() - b->elements[i].get_number()));
        return Value(static_cast<Obj*>(r));
    });
    vm.define_native("vec3_scale", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_number()) return Value();
        auto* v = argv[0].as_array(); double s = argv[1].get_number(); auto* r = allocate_array();
        for (int i = 0; i < 3; i++) r->elements.push_back(Value(v->elements[i].get_number() * s));
        return Value(static_cast<Obj*>(r));
    });
    vm.define_native("vec3_dot", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_array()) return Value();
        auto* a = argv[0].as_array(); auto* b = argv[1].as_array();
        return Value(a->elements[0].get_number() * b->elements[0].get_number() + a->elements[1].get_number() * b->elements[1].get_number() + a->elements[2].get_number() * b->elements[2].get_number());
    });
    vm.define_native("vec3_cross", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_array()) return Value();
        auto* a = argv[0].as_array(); auto* b = argv[1].as_array();
        double ax = a->elements[0].get_number(), ay = a->elements[1].get_number(), az = a->elements[2].get_number();
        double bx = b->elements[0].get_number(), by = b->elements[1].get_number(), bz = b->elements[2].get_number();
        auto* r = allocate_array();
        r->elements.push_back(Value(ay * bz - az * by)); r->elements.push_back(Value(az * bx - ax * bz)); r->elements.push_back(Value(ax * by - ay * bx));
        return Value(static_cast<Obj*>(r));
    });
    vm.define_native("vec3_len", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_array()) return Value();
        auto* v = argv[0].as_array();
        double x = v->elements[0].get_number(), y = v->elements[1].get_number(), z = v->elements[2].get_number();
        return Value(std::sqrt(x * x + y * y + z * z));
    });
    vm.define_native("vec3_normalize", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_array()) return Value();
        auto* v = argv[0].as_array();
        double x = v->elements[0].get_number(), y = v->elements[1].get_number(), z = v->elements[2].get_number();
        double len = std::sqrt(x * x + y * y + z * z);
        if (len == 0) return Value(static_cast<Obj*>(allocate_array()));
        auto* r = allocate_array(); r->elements.push_back(Value(x / len)); r->elements.push_back(Value(y / len)); r->elements.push_back(Value(z / len));
        return Value(static_cast<Obj*>(r));
    });
    vm.define_native("vec3_dist", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_array() || !argv[1].is_array()) return Value();
        auto* a = argv[0].as_array(); auto* b = argv[1].as_array();
        double dx = a->elements[0].get_number() - b->elements[0].get_number();
        double dy = a->elements[1].get_number() - b->elements[1].get_number();
        double dz = a->elements[2].get_number() - b->elements[2].get_number();
        return Value(std::sqrt(dx * dx + dy * dy + dz * dz));
    });
}

} // namespace akar
