#pragma once
#include "akar/common/value.h"
#include <string>
#include <functional>

namespace akar {

class VM;

// Register a native function that can be called from Akar script
void register_native_function(VM& vm, const std::string& name, NativeFn fn);

// Built-in native functions
void register_builtins(VM& vm);

// Utility to convert C++ types to/from Value
template<typename T> Value to_value(T val);
template<> Value to_value<double>(double val);
template<> Value to_value<int>(int val);
template<> Value to_value<bool>(bool val);
template<> Value to_value<std::string>(std::string val);

double value_to_double(Value v);
int value_to_int(Value v);
bool value_to_bool(Value v);
std::string value_to_string(Value v);

} // namespace akar
