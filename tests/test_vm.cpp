#include "akar/vm/vm.h"

TEST(vm_basic_arithmetic) {
    akar::VM vm;
    auto result = vm.interpret("let x = 10 + 20 * 2");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(vm_string_concatenation) {
    akar::VM vm;
    auto result = vm.interpret("let s = \"hello\" + \" \" + \"world\"");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(vm_if_else) {
    akar::VM vm;
    auto result = vm.interpret("let x = 10\nif (x > 5) { x = x + 1 } else { x = x - 1 }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(vm_while_loop) {
    akar::VM vm;
    auto result = vm.interpret("let i = 0\nwhile (i < 10) { i = i + 1 }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(vm_for_in_range) {
    akar::VM vm;
    auto result = vm.interpret("for i in 0..5 { }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(vm_function_call) {
    akar::VM vm;
    auto result = vm.interpret("fn add(a, b) { return a + b }\nlet x = add(3, 4)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(vm_array_operations) {
    akar::VM vm;
    auto result = vm.interpret("let arr = [1, 2, 3]\nlet x = arr[1]");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(vm_closure) {
    // Simple closure: fn that captures outer variable via assignment
    akar::VM vm;
    auto result = vm.interpret("let x = 10\nfn get_x() { return x }\nlet result = get_x()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(vm_class_basic) {
    akar::VM vm;
    auto result = vm.interpret("class Point {\n  init(x, y) {\n    this.x = x\n    this.y = y\n  }\n}\nlet p = Point(1, 2)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}
