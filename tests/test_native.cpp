#include "akar/vm/vm.h"
#include "akar/vm/native.h"

TEST(native_print) {
    akar::VM vm;
    auto result = vm.interpret("print(\"hello\")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(native_len) {
    akar::VM vm;
    auto result = vm.interpret("let arr = [1, 2, 3]\nlet n = len(arr)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(native_type) {
    akar::VM vm;
    auto result = vm.interpret("let t = type(42)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(native_to_number) {
    akar::VM vm;
    auto result = vm.interpret("let n = to_number(\"123\")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(native_to_string) {
    akar::VM vm;
    auto result = vm.interpret("let s = to_string(123)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(native_push_pop) {
    akar::VM vm;
    auto result = vm.interpret("let arr = [1, 2]\npush(arr, 3)\nlet x = pop(arr)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(native_math) {
    akar::VM vm;
    auto result = vm.interpret("let a = floor(3.7)\nlet b = ceil(3.2)\nlet c = abs(-5)\nlet d = sqrt(16)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(cpp_call_script) {
    akar::VM vm;
    vm.interpret("fn double(x) { return x * 2 }");
    auto* closure = vm.get_global("double").as_closure();
    ASSERT_TRUE(closure != nullptr);
    auto result = vm.call_function(closure, {akar::Value(21.0)});
    ASSERT_TRUE(result.is_number());
    ASSERT_EQ(result.get_number(), 42.0);
}

TEST(fiber_create) {
    akar::VM vm;
    auto result = vm.interpret("fn gen() { return 42 }\nlet f = fiber_create(gen)\nlet t = type(f)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    // fiber should be created successfully
    auto fiber = vm.get_global("f");
    ASSERT_TRUE(fiber.is_fiber());
}

TEST(fiber_resume_basic) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn gen(val) { return val }\n"
        "let f = fiber_create(gen)\n"
        "let r = fiber_resume(f, 42)\n"
    );
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto r = vm.get_global("r");
    ASSERT_TRUE(r.is_number());
    ASSERT_EQ(r.get_number(), 42.0);
}

TEST(fiber_status) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn gen() { return 1 }\n"
        "let f = fiber_create(gen)\n"
        "let s1 = fiber_status(f)\n"
        "let r = fiber_resume(f, nil)\n"
        "let s2 = fiber_status(f)\n"
    );
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}
