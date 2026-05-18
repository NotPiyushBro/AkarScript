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

// ============================================================
// Comprehensive opcode tests
// ============================================================

// Test: ADD with numbers and strings
TEST(opcode_add) {
    akar::VM vm;
    auto result = vm.interpret("let a = 3 + 4\nlet b = \"hello\" + \" world\"");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: SUB, MUL, DIV, MOD
TEST(opcode_arithmetic) {
    akar::VM vm;
    auto result = vm.interpret("let a = 10 - 3\nlet b = 4 * 5\nlet c = 20 / 4\nlet d = 17 % 5");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: NEG
TEST(opcode_neg) {
    akar::VM vm;
    auto result = vm.interpret("let a = 5\nlet b = -a");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: EQ, NEQ
TEST(opcode_equality) {
    akar::VM vm;
    auto result = vm.interpret("let a = 1 == 1\nlet b = 1 != 2\nlet c = 1 == 2\nlet d = 1 != 1\nlet e = \"x\" == \"x\"\nlet f = \"x\" != \"y\"");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: LT, LTE, GT, GTE
TEST(opcode_comparison) {
    akar::VM vm;
    auto result = vm.interpret("let a = 1 < 2\nlet b = 2 <= 2\nlet c = 3 > 1\nlet d = 3 >= 3\nlet e = 2 < 1\nlet f = 1 >= 2");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String comparisons
TEST(opcode_string_comparison) {
    akar::VM vm;
    auto result = vm.interpret("let a = \"a\" < \"b\"\nlet b = \"abc\" <= \"abc\"\nlet c = \"z\" > \"a\"\nlet d = \"abc\" >= \"abd\"");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: NOT
TEST(opcode_not) {
    akar::VM vm;
    auto result = vm.interpret("let a = !true\nlet b = !false\nlet c = !nil\nlet d = !0");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: LOAD_CONST, LOAD_NIL, LOAD_TRUE, LOAD_FALSE
TEST(opcode_load_constants) {
    akar::VM vm;
    auto result = vm.interpret("let a = nil\nlet b = true\nlet c = false\nlet d = 42\nlet e = 3.14");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: MOVE (via assignment)
TEST(opcode_move) {
    akar::VM vm;
    auto result = vm.interpret("let a = 10\nlet b = a\nlet c = b");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: GET_LOCAL, SET_LOCAL
TEST(opcode_local_access) {
    akar::VM vm;
    auto result = vm.interpret("let x = 5\nlet y = x + 1\nx = y * 2");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: GET_GLOBAL, SET_GLOBAL
TEST(opcode_global_access) {
    akar::VM vm;
    auto result = vm.interpret("let x = 10\nfn get() { return x }\nfn set() { x = 20 }\nset()\nlet y = get()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: JMP (unconditional jump in while loop)
TEST(opcode_jmp) {
    akar::VM vm;
    auto result = vm.interpret("let i = 0\nwhile (i < 100) { i = i + 1 }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: JMP_IF_FALSE (if/else)
TEST(opcode_jmp_if_false) {
    akar::VM vm;
    auto result = vm.interpret("let x = 5\nlet y = 0\nif (x > 3) { y = 1 } else { y = 2 }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: JMP_IF_TRUE (short-circuit or)
TEST(opcode_jmp_if_true) {
    akar::VM vm;
    auto result = vm.interpret("let a = true\nlet b = false\nlet c = a or b\nlet d = b or a");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: CALL, RETURN
TEST(opcode_call_return) {
    akar::VM vm;
    auto result = vm.interpret("fn add(a, b) { return a + b }\nlet x = add(3, 4)\nlet y = add(x, 1)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: CLOSURE (capture upvalue)
TEST(opcode_closure) {
    akar::VM vm;
    auto result = vm.interpret("let x = 10\nfn make() { fn inner() { return x } return inner }\nlet f = make()\nlet r = f()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: NEW_ARRAY, GET_INDEX, SET_INDEX
TEST(opcode_array) {
    akar::VM vm;
    auto result = vm.interpret("let arr = [10, 20, 30]\nlet x = arr[1]\narr[0] = 99\nlet y = arr[0]");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: NEW_MAP, GET_FIELD, SET_FIELD
TEST(opcode_map) {
    akar::VM vm;
    auto result = vm.interpret("let m = {\"a\": 1, \"b\": 2}\nlet x = m.a\nm.c = 3");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: NEW_CLASS, NEW_INSTANCE, GET_METHOD
TEST(opcode_class) {
    akar::VM vm;
    auto result = vm.interpret(
        "class Counter {\n"
        "  init(n) { this.n = n }\n"
        "  inc() { this.n = this.n + 1 }\n"
        "  get() { return this.n }\n"
        "}\n"
        "let c = Counter(0)\nc.inc()\nc.inc()\nlet r = c.get()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: NEW_RANGE
TEST(opcode_range) {
    akar::VM vm;
    auto result = vm.interpret("let r = 0..10");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: ITER_INIT, ITER_NEXT, ITER_DONE (for-in with range)
TEST(opcode_iterator_range) {
    akar::VM vm;
    auto result = vm.interpret("let sum = 0\nfor i in 0..100 { sum = sum + i }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: ITER_INIT, ITER_NEXT, ITER_DONE (for-in with array)
TEST(opcode_iterator_array) {
    akar::VM vm;
    auto result = vm.interpret("let arr = [10, 20, 30]\nlet sum = 0\nfor x in arr { sum = sum + x }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: ITER_INIT, ITER_NEXT, ITER_DONE (for-in with string)
TEST(opcode_iterator_string) {
    akar::VM vm;
    auto result = vm.interpret("let s = \"hello\"\nlet count = 0\nfor ch in s { count = count + 1 }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: PRINT
TEST(opcode_print) {
    akar::VM vm;
    auto result = vm.interpret("print(42)\nprint(\"test\")\nprint(nil)\nprint(true)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: TRY_BEGIN, TRY_END, THROW
TEST(opcode_try_catch) {
    akar::VM vm;
    auto result = vm.interpret(
        "let x = 0\n"
        "try {\n"
        "  x = 1\n"
        "  throw \"error\"\n"
        "  x = 999\n"
        "} catch (e) {\n"
        "  x = 2\n"
        "}\n"
        "let done = true");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: FIBER_YIELD, FIBER_RESUME (via native functions)
TEST(opcode_fiber) {
    akar::VM vm;
    // fiber_create creates a fiber, fiber_resume resumes it
    // The generator function uses fiber_yield to yield values
    auto result = vm.interpret(
        "fn gen() {\n"
        "  fiber_yield(1)\n"
        "  fiber_yield(2)\n"
        "  return 3\n"
        "}\n"
        "let f = fiber_create(gen)\n"
        "let s = fiber_status(f)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: TAIL_CALL
TEST(opcode_tail_call) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn countdown(n) {\n"
        "  if (n <= 0) { return 0 }\n"
        "  return countdown(n - 1)\n"
        "}\n"
        "let r = countdown(100)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: CLOSE_UPVALUE
TEST(opcode_close_upvalue) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn make_counter() {\n"
        "  let count = 0\n"
        "  fn inc() { count = count + 1 return count }\n"
        "  return inc\n"
        "}\n"
        "let c = make_counter()\n"
        "let a = c()\n"
        "let b = c()\n"
        "let d = c()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Quickened ADD_NUM (arithmetic in a tight loop)
TEST(opcode_quickened_add) {
    akar::VM vm;
    auto result = vm.interpret("let sum = 0\nfor i in 0..1000 { sum = sum + i }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Quickened comparison opcodes (LT_NUM etc in loop)
TEST(opcode_quickened_cmp) {
    akar::VM vm;
    auto result = vm.interpret(
        "let count = 0\n"
        "let i = 0\n"
        "while (i < 1000) {\n"
        "  if (i < 500) { count = count + 1 }\n"
        "  i = i + 1\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String concatenation (ADD_STR quickening)
TEST(opcode_quickened_str) {
    akar::VM vm;
    auto result = vm.interpret("let s = \"\"\nfor i in 0..10 { s = s + \"x\" }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Mixed arithmetic (all ops in one expression)
TEST(opcode_mixed_arithmetic) {
    akar::VM vm;
    auto result = vm.interpret("let a = (10 + 5) * 2 - 8 / 4\nlet b = a % 7\nlet c = -b");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Nested function calls
TEST(opcode_nested_calls) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn double(x) { return x * 2 }\n"
        "fn add(a, b) { return a + b }\n"
        "let r = add(double(3), double(4))");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Deeply nested closures
TEST(opcode_deep_closure) {
    akar::VM vm;
    auto result = vm.interpret(
        "let x = 1\n"
        "fn a() { let y = 2\nfn b() { let z = 3\nfn c() { return x + y + z } return c } return b }\n"
        "let f = a()()\nlet r = f()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Switch statement
TEST(opcode_switch) {
    akar::VM vm;
    auto result = vm.interpret(
        "let x = 2\n"
        "let r = 0\n"
        "switch (x) {\n"
        "  case 1: r = 10 break\n"
        "  case 2: r = 20 break\n"
        "  case 3: r = 30 break\n"
        "  default: r = 99\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Destructuring
TEST(opcode_destructuring) {
    akar::VM vm;
    auto result = vm.interpret("let [a, b, c] = [10, 20, 30]\nlet sum = a + b + c");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Varargs function
TEST(opcode_varargs) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn sum(...args) {\n"
        "  let total = 0\n"
        "  for a in args { total = total + a }\n"
        "  return total\n"
        "}\n"
        "let r = sum(1, 2, 3, 4, 5)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Comprehensive arithmetic with quickening verification
TEST(opcode_quickening_full) {
    akar::VM vm;
    // This exercises all quickened opcodes in a tight loop
    auto result = vm.interpret(
        "fn test() {\n"
        "  let sum = 0\n"
        "  let prod = 1\n"
        "  for i in 1..100 {\n"
        "    sum = sum + i\n"
        "    prod = prod + 1\n"
        "    if (i > 50) { sum = sum - 1 }\n"
        "    if (i < 60) { sum = sum + 0 }\n"
        "    if (i >= 40) { sum = sum + 0 }\n"
        "    if (i <= 70) { sum = sum + 0 }\n"
        "    if (i == 50) { sum = sum + 0 }\n"
        "    if (i != 99) { sum = sum + 0 }\n"
        "  }\n"
        "  return sum\n"
        "}\n"
        "let r = test()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}
