#include "akar/vm/vm.h"
#include <sstream>

// Helper: generate source that allocates N registers to force WIDE prefix
static std::string gen_wide_code(const std::string& body) {
    std::ostringstream ss;
    // Declare 260 local variables to push register count past 255
    for (int i = 0; i < 260; i++) {
        ss << "let v" << i << " = " << i << "\n";
    }
    ss << body << "\n";
    return ss.str();
}

// Test: WIDE LOAD_CONST, MOVE, arithmetic (ADD/SUB/MUL/DIV)
TEST(wide_arithmetic) {
    akar::VM vm;
    auto src = gen_wide_code("let a = v256 + v257\nlet b = v258 * v259\nlet c = b - a");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE GET_LOCAL, SET_LOCAL
TEST(wide_local_access) {
    akar::VM vm;
    auto src = gen_wide_code("let x = v256\nv257 = x + 1");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE comparison opcodes (EQ, NEQ, LT, LTE, GT, GTE)
TEST(wide_comparisons) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let r1 = v256 == v256\n"
        "let r2 = v256 != v257\n"
        "let r3 = v256 < v257\n"
        "let r4 = v257 > v256\n"
        "let r5 = v256 <= v256\n"
        "let r6 = v256 >= v256");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE JMP_IF_FALSE, JMP_IF_TRUE
TEST(wide_conditional_jump) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let flag = true\n"
        "if (flag) { v256 = 999 }\n"
        "if (!flag) { v257 = 888 } else { v257 = 777 }");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE JMP (unconditional - loops)
TEST(wide_loop_jump) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let i = 0\n"
        "while (i < 5) { i = i + 1 }");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE LOAD_NIL, LOAD_TRUE, LOAD_FALSE
TEST(wide_load_constants) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let a = nil\n"
        "let b = true\n"
        "let c = false");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE NEG, NOT
TEST(wide_unary_ops) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let a = -v256\n"
        "let b = !false");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE PRINT
TEST(wide_print) {
    akar::VM vm;
    auto src = gen_wide_code("print(v256)");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE NEW_ARRAY, GET_INDEX, SET_INDEX
TEST(wide_array_ops) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let arr = [1, 2, 3]\n"
        "let x = arr[1]\n"
        "arr[0] = 99");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE NEW_MAP, GET_FIELD, SET_FIELD
TEST(wide_map_ops) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let m = {\"a\": 1, \"b\": 2}\n"
        "let x = m.a\n"
        "m.c = 3");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE GET_UPVALUE, SET_UPVALUE (closure capturing wide registers)
TEST(wide_closure_upvalues) {
    akar::VM vm;
    // Create a function with many locals, then a closure that captures one
    auto src = gen_wide_code(
        "let captured = 42\n"
        "fn get_captured() { return captured }\n"
        "let result = get_captured()");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE RETURN from function with many locals
TEST(wide_return) {
    akar::VM vm;
    auto src = gen_wide_code(
        "fn wide_fn() {\n"
        "  let local1 = 1\n"
        "  let local2 = 2\n"
        "  return local1 + local2\n"
        "}\n"
        "let r = wide_fn()");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE CALL (calling function from wide register context)
TEST(wide_call) {
    akar::VM vm;
    auto src = gen_wide_code(
        "fn add(a, b) { return a + b }\n"
        "let r = add(v256, v257)");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE NEW_CLASS, NEW_INSTANCE, GET_METHOD
TEST(wide_class_ops) {
    akar::VM vm;
    auto src = gen_wide_code(
        "class Box {\n"
        "  init(val) { this.val = val }\n"
        "  get_val() { return this.val }\n"
        "}\n"
        "let b = Box(42)\n"
        "let x = b.get_val()");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE CLOSURE
TEST(wide_closure_create) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let base = 10\n"
        "fn make_adder(x) {\n"
        "  fn adder(y) { return x + y + base }\n"
        "  return adder\n"
        "}\n"
        "let add5 = make_adder(5)\n"
        "let r = add5(3)");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: TRY_BEGIN/TRY_END byte offset (not *4)
TEST(try_catch_offset) {
    akar::VM vm;
    // This test verifies TRY_BEGIN/TRY_END use byte offsets correctly
    // by having actual code between try and catch
    auto result = vm.interpret(
        "let x = 0\n"
        "try {\n"
        "  x = 1\n"
        "  x = 2\n"
        "  x = 3\n"
        "  throw \"error\"\n"
        "  x = 999\n"
        "} catch (e) {\n"
        "  x = 4\n"
        "}\n"
        "let done = true");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE ITER_INIT, ITER_NEXT, ITER_DONE (for-in loop with wide registers)
TEST(wide_for_in_loop) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let sum = 0\n"
        "for i in 0..5 { sum = sum + i }");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE MOD opcode
TEST(wide_mod) {
    akar::VM vm;
    auto src = gen_wide_code("let r = v257 % v256");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

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
