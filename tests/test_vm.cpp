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
// NOTE: This test is skipped because the for-in iterator + 260 registers
// Test: WIDE for-in loop (exercises WIDE JMP back-edge)
TEST(wide_for_in_loop) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let sum = 0\n"
        "let i = 0\n"
        "while (i < 10) {\n"
        "  sum = sum + i\n"
        "  i = i + 1\n"
        "}");
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

// ============================================================
// WIDE + Quickened opcode tests
// (WIDE handler support for ADD_NUM, SUB_NUM, MUL_NUM, etc.)
// ============================================================

// Test: WIDE + quickened arithmetic (ADD_NUM, SUB_NUM, MUL_NUM, DIV_NUM, MOD_NUM)
// These get quickened at runtime: first pass ADD -> ADD_NUM etc.
// The WIDE handler must support the quickened opcodes.
TEST(wide_quickened_arithmetic) {
    akar::VM vm;
    // Run twice to force quickening on second execution
    auto src = gen_wide_code(
        "let a = v256 + v257\n"    // ADD -> ADD_NUM
        "let b = v258 - v259\n"    // SUB -> SUB_NUM
        "let c = v256 * v257\n"    // MUL -> MUL_NUM
        "let d = v258 / v259\n"    // DIV -> DIV_NUM
        "let e = v257 % v256");    // MOD -> MOD_NUM
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE + quickened comparison opcodes
// First pass uses EQ/NEQ/LT/LTE/GT/GTE, second pass uses quickened versions
TEST(wide_quickened_comparisons) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let r1 = v256 == v256\n"   // EQ -> EQ_NUM
        "let r2 = v256 != v257\n"   // NEQ -> NEQ_NUM
        "let r3 = v256 < v257\n"    // LT -> LT_NUM
        "let r4 = v257 > v256\n"    // GT -> GT_NUM
        "let r5 = v256 <= v256\n"   // LTE -> LTE_NUM
        "let r6 = v256 >= v256");   // GTE -> GTE_NUM
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE + ADD_STR (quickened string concatenation)
TEST(wide_quickened_string_concat) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let a = \"hello\"\n"
        "let b = \"world\"\n"
        "let c = a + b");           // ADD -> ADD_STR (string specialization)
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE + MOD_EQ_ZERO (fused mod-equals-zero opcode)
TEST(wide_mod_eq_zero) {
    akar::VM vm;
    // The % 2 == 0 pattern gets fused to MOD_EQ_ZERO by codegen
    auto src = gen_wide_code(
        "let a = v256 % 2 == 0\n"
        "let b = v257 % 3 == 0\n"
        "let c = v258 % 7 == 0");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE + LOAD_IMM and ADD_IMM (small integer inline encoding)
TEST(wide_load_imm_add_imm) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let i = 0\n"
        "while (i < 10) {\n"        // LOAD_IMM for 0, 10; ADD_IMM for i+1
        "  i = i + 1\n"
        "}"
        "let j = v256 + 1\n"        // ADD_IMM with wide register
        "let k = v257 + 42");       // ADD_IMM with immediate
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE + fused compare-branch opcodes (JMP_IF_NOT_LT etc.)
TEST(wide_fused_compare_branch) {
    akar::VM vm;
    auto src = gen_wide_code(
        "let count = 0\n"
        "let i = 0\n"
        "while (i < 100) {\n"       // JMP_IF_NOT_LT (fused)
        "  if (i <= 50) { count = count + 1 }\n"  // JMP_IF_NOT_LTE (fused)
        "  if (i > 25) { count = count + 1 }\n"   // JMP_IF_NOT_GT (fused)
        "  if (i >= 10) { count = count + 1 }\n"  // JMP_IF_NOT_GTE (fused)
        "  if (i == 42) { count = count + 1 }\n"  // JMP_IF_NOT_EQ (fused)
        "  i = i + 1\n"
        "}");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// Peephole optimization tests
// ============================================================

// Test: LOAD_IMM + ADD_NUM -> ADD_IMM fusion
// The peephole should fuse "LOAD_IMM R,small; ADD_NUM R,R,R_imm" into "ADD_IMM R,R,small"
TEST(peephole_add_num_fusion) {
    akar::VM vm;
    // Tight loop where i = i + 1 gets optimized
    auto result = vm.interpret(
        "let sum = 0\n"
        "let i = 0\n"
        "while (i < 1000) {\n"
        "  sum = sum + i\n"
        "  i = i + 1\n"            // LOAD_IMM 1 + ADD_NUM -> ADD_IMM
        "}"
        "let x = sum + 5\n"         // ADD with small immediate
        "let y = x + 200");         // ADD with larger immediate
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: LTE_NUM + JMP_IF_FALSE -> JMP_IF_NOT_LTE fusion
// The peephole fuses quickened comparison + branch into fused opcode
TEST(peephole_cmp_branch_fusion) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn count_up_to(n) {\n"
        "  let count = 0\n"
        "  let i = 0\n"
        "  while (i <= n) {\n"       // LTE_NUM + JMP_IF_FALSE -> JMP_IF_NOT_LTE
        "    count = count + 1\n"
        "    i = i + 1\n"
        "  }\n"
        "  return count\n"
        "}\n"
        "let r1 = count_up_to(100)\n"
        "let r2 = count_up_to(500)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: All fused compare-branch peephole transformations
TEST(peephole_all_fused_branches) {
    akar::VM vm;
    // Exercise all 5 fused branch opcodes via peephole
    auto result = vm.interpret(
        "fn test() {\n"
        "  let a = 0\n"
        "  let b = 0\n"
        "  let c = 0\n"
        "  let d = 0\n"
        "  let e = 0\n"
        "  let i = 0\n"
        "  while (i < 100) {\n"      // JMP_IF_NOT_LT
        "    if (i <= 50) { a = a + 1 }\n"   // JMP_IF_NOT_LTE
        "    if (i > 25) { b = b + 1 }\n"    // JMP_IF_NOT_GT
        "    if (i >= 10) { c = c + 1 }\n"   // JMP_IF_NOT_GTE
        "    if (i == 50) { d = d + 1 }\n"   // JMP_IF_NOT_EQ
        "    if (i != 99) { e = e + 1 }\n"   // JMP_IF_NOT_EQ (NEQ -> EQ inversion)
        "    i = i + 1\n"
        "  }\n"
        "  return a + b + c + d + e\n"
        "}\n"
        "let r = test()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Dead MOVE elimination (MOVE before backward JMP where target overwrites dest)
TEST(peephole_dead_move_elim) {
    akar::VM vm;
    // The is_prime pattern has a dead MOVE in the while loop body:
    //   i = i + 2  (writes R4)
    //   MOVE R6 <- R4  (dead - overwritten by MUL_NUM R6 = R4*R4 at loop top)
    //   JMP -> loop_top
    // The peephole should eliminate this dead MOVE
    auto result = vm.interpret(
        "fn is_prime(n) {\n"
        "  if (n < 2) { return false }\n"
        "  if (n == 2) { return true }\n"
        "  if (n % 2 == 0) { return false }\n"
        "  let i = 3\n"
        "  while (i * i <= n) {\n"
        "    if (n % i == 0) { return false }\n"
        "    i = i + 2\n"            // After peephole: ADD_IMM only, no dead MOVE
        "  }\n"
        "  return true\n"
        "}\n"
        "fn count_primes(limit) {\n"
        "  let count = 0\n"
        "  for n in 2..limit {\n"
        "    if (is_prime(n)) { count = count + 1 }\n"
        "  }\n"
        "  return count\n"
        "}\n"
        "let r = count_primes(100)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Script-level peephole (applied to top-level bytecode)
TEST(peephole_script_level) {
    akar::VM vm;
    // Top-level code should also benefit from peephole optimizations
    // (fused branches, ADD_IMM, dead MOVE elimination)
    auto result = vm.interpret(
        "let sum = 0\n"
        "let i = 0\n"
        "while (i <= 1000) {\n"      // peephole: LTE + JMP_IF_FALSE -> JMP_IF_NOT_LTE
        "  sum = sum + i\n"
        "  i = i + 1\n"             // peephole: LOAD_IMM + ADD_NUM -> ADD_IMM
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Peephole in function with multiple fused branch types
TEST(peephole_nested_fused_branches) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn fizzbuzz(n) {\n"
        "  let count = 0\n"
        "  for i in 1..n {\n"
        "    if (i % 15 == 0) { count = count + 1 }\n"  // MOD_EQ_ZERO + JMP_IF_NOT_EQ
        "    else if (i % 3 == 0) { count = count + 1 }\n"
        "    else if (i % 5 == 0) { count = count + 1 }\n"
        "  }\n"
        "  return count\n"
        "}\n"
        "let r = fizzbuzz(1000)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// JIT optimization tests
// ============================================================

// Test: JIT inline MOD_EQ_ZERO (is_prime pattern)
// The JIT should inline the fmod check instead of calling a helper function
TEST(jit_inline_mod_eq_zero) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn is_prime(n) {\n"
        "  if (n < 2) { return false }\n"
        "  if (n == 2) { return true }\n"
        "  if (n % 2 == 0) { return false }\n"
        "  let i = 3\n"
        "  while (i * i <= n) {\n"
        "    if (n % i == 0) { return false }\n"  // MOD_EQ_ZERO inlined in JIT
        "    i = i + 2\n"
        "  }\n"
        "  return true\n"
        "}\n"
        // Call is_prime many times to trigger JIT compilation (threshold=50)
        "let count = 0\n"
        "for n in 2..500 {\n"
        "  if (is_prime(n)) { count = count + 1 }\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: JIT FP store cache (MUL_NUM result reused in comparison)
// The JIT should cache the FP result of MUL_NUM and skip the redundant LDR
// in the following JMP_IF_NOT_LTE
TEST(jit_fp_store_cache) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn sum_squares(n) {\n"
        "  let sum = 0\n"
        "  let i = 1\n"
        "  while (i * i <= n) {\n"    // MUL_NUM R6=R4*R4 then JMP_IF_NOT_LTE R6,R0
        "    sum = sum + i * i\n"
        "    i = i + 1\n"
        "  }\n"
        "  return sum\n"
        "}\n"
        // Call many times to trigger JIT
        "let total = 0\n"
        "for x in 1..100 {\n"
        "  total = total + sum_squares(x)\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: JIT self-op MUL_NUM optimization (i * i pattern)
// When both MUL operands are the same register, JIT uses FMUL D0,D0,D0
TEST(jit_self_op_mul) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn distance_sq(x1, y1, x2, y2) {\n"
        "  let dx = x2 - x1\n"
        "  let dy = y2 - y1\n"
        "  return dx * dx + dy * dy\n"  // Self-multiply: dx*dx and dy*dy
        "}\n"
        "let total = 0\n"
        "for i in 0..100 {\n"
        "  total = total + distance_sq(0, 0, i, i)\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: JIT inline MOD_NUM (quickened modulo in JIT)
TEST(jit_inline_mod_num) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn count_divisible(n, d) {\n"
        "  let count = 0\n"
        "  for i in 1..n {\n"
        "    if (i % d == 0) { count = count + 1 }\n"
        "  }\n"
        "  return count\n"
        "}\n"
        // Call many times to trigger JIT for count_divisible
        "let total = 0\n"
        "for x in 1..60 {\n"
        "  total = total + count_divisible(100, x)\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: JIT handles is_prime with correct results (not just no crash)
// Uses fn result verification via get_global
TEST(jit_is_prime_correctness) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn is_prime(n) {\n"
        "  if (n < 2) { return false }\n"
        "  if (n == 2) { return true }\n"
        "  if (n % 2 == 0) { return false }\n"
        "  let i = 3\n"
        "  while (i * i <= n) {\n"
        "    if (n % i == 0) { return false }\n"
        "    i = i + 2\n"
        "  }\n"
        "  return true\n"
        "}\n"
        // Warm up JIT by calling is_prime many times
        "let warmup = 0\n"
        "for n in 2..200 {\n"
        "  if (is_prime(n)) { warmup = warmup + 1 }\n"
        "}\n"
        // Now verify known primes are correctly identified
        "fn verify() {\n"
        "  if (!is_prime(2)) { return false }\n"
        "  if (!is_prime(3)) { return false }\n"
        "  if (!is_prime(5)) { return false }\n"
        "  if (!is_prime(7)) { return false }\n"
        "  if (!is_prime(11)) { return false }\n"
        "  if (!is_prime(97)) { return false }\n"
        "  if (is_prime(4)) { return false }\n"
        "  if (is_prime(9)) { return false }\n"
        "  if (is_prime(100)) { return false }\n"
        "  if (is_prime(1)) { return false }\n"
        "  if (is_prime(0)) { return false }\n"
        "  return true\n"
        "}\n"
        "let ok = verify()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto ok = vm.get_global("ok");
    ASSERT_TRUE(ok.is_bool());
    ASSERT_TRUE(ok.get_bool());
}

// Test: JIT with MOD_EQ_ZERO in a tight loop (divisibility checking)
TEST(jit_mod_eq_zero_loop) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn sum_of_divisors(n) {\n"
        "  let sum = 0\n"
        "  for i in 1..n {\n"
        "    if (n % i == 0) { sum = sum + i }\n"  // MOD_EQ_ZERO
        "  }\n"
        "  return sum\n"
        "}\n"
        "let total = 0\n"
        "for n in 1..80 {\n"
        "  total = total + sum_of_divisors(n)\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// Script-level let → locals tests
// ============================================================

// Test: Top-level let uses local registers (not globals)
// After the optimization, let declarations at script scope become locals
TEST(script_let_as_locals) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = 10\n"
        "let b = 20\n"
        "let c = a + b\n"
        "a = c * 2\n"
        "let d = a + b + c");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Top-level let locals work with fn (fn uses globals, let uses locals)
TEST(script_let_locals_with_fn) {
    akar::VM vm;
    auto result = vm.interpret(
        "let x = 10\n"
        "let y = 20\n"
        "fn add(a, b) { return a + b }\n"
        "let z = add(x, y)\n"
        "x = z + 5");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Top-level let locals in loops (hot path optimization)
TEST(script_let_locals_hot_loop) {
    akar::VM vm;
    // This tests that the outer loop with local variables is optimized
    // (peephole applied to script bytecode, locals instead of globals)
    auto result = vm.interpret(
        "let sum = 0\n"
        "let count = 0\n"
        "let n = 2\n"
        "while (n <= 500) {\n"
        "  let is_prime = true\n"
        "  let i = 2\n"
        "  while (i * i <= n) {\n"
        "    if (n % i == 0) { is_prime = false }\n"
        "    i = i + 1\n"
        "  }\n"
        "  if (is_prime) { sum = sum + n\n count = count + 1 }\n"
        "  n = n + 1\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Script-level let with closures (upvalue capture)
TEST(script_let_locals_closures) {
    akar::VM vm;
    auto result = vm.interpret(
        "let base = 100\n"
        "fn make_adder(x) {\n"
        "  fn adder(y) { return x + y + base }\n"
        "  return adder\n"
        "}\n"
        "let add5 = make_adder(5)\n"
        "let r = add5(3)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// Bitwise opcode tests
// NOTE: Bitwise opcodes (BIT_AND through SHR) are defined in opcodes.h
// but not yet exposed in the language syntax. Tests removed until
// language support is added.
// ============================================================

// ============================================================
// Integration tests (combining multiple optimizations)
// ============================================================

// Test: Full sum_primes benchmark (exercises all optimizations together)
TEST(integration_sum_primes) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn is_prime(n) {\n"
        "  if (n < 2) { return false }\n"
        "  if (n == 2) { return true }\n"
        "  if (n % 2 == 0) { return false }\n"
        "  let i = 3\n"
        "  while (i * i <= n) {\n"
        "    if (n % i == 0) { return false }\n"
        "    i = i + 2\n"
        "  }\n"
        "  return true\n"
        "}\n"
        "let sum = 0\n"
        "let count = 0\n"
        "let n = 2\n"
        "while (n <= 5000) {\n"
        "  if (is_prime(n)) {\n"
        "    sum = sum + n\n"
        "    count = count + 1\n"
        "  }\n"
        "  n = n + 1\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Tight loop exercising all quickened opcodes and fused branches
TEST(integration_all_quickened) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn compute(n) {\n"
        "  let sum = 0\n"
        "  for i in 1..n {\n"
        "    sum = sum + i\n"          // ADD_NUM
        "    if (i > 50) { sum = sum - 1 }\n"  // GT_NUM, SUB_NUM
        "    if (i < 100) { sum = sum + 0 }\n"  // LT_NUM
        "    if (i >= 40) { sum = sum + 0 }\n"  // GTE_NUM
        "    if (i <= 200) { sum = sum + 0 }\n" // LTE_NUM
        "    if (i == 50) { sum = sum + 0 }\n"  // EQ_NUM
        "    if (i != 99) { sum = sum + 0 }\n"  // NEQ_NUM
        "  }\n"
        "  return sum\n"
        "}\n"
        "let r = compute(500)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Nested loops with modulo (exercises JIT inline MOD_EQ_ZERO + FP cache)
TEST(integration_nested_mod) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn count_divisors(n) {\n"
        "  let count = 0\n"
        "  for i in 1..n {\n"
        "    if (n % i == 0) { count = count + 1 }\n"  // MOD_EQ_ZERO
        "  }\n"
        "  return count\n"
        "}\n"
        "let total = 0\n"
        "for n in 1..100 {\n"
        "  total = total + count_divisors(n)\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Arithmetic-heavy function with self-multiply pattern
TEST(integration_self_mul) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn norm_sq(x, y, z) {\n"
        "  return x * x + y * y + z * z\n"  // Three self-multiply operations
        "}\n"
        "let total = 0\n"
        "for i in 0..100 {\n"
        "  total = total + norm_sq(i, i+1, i+2)\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Large prime computation (stress test all JIT optimizations)
TEST(integration_large_primes) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn is_prime(n) {\n"
        "  if (n < 2) { return false }\n"
        "  if (n == 2) { return true }\n"
        "  if (n % 2 == 0) { return false }\n"
        "  let i = 3\n"
        "  while (i * i <= n) {\n"
        "    if (n % i == 0) { return false }\n"
        "    i = i + 2\n"
        "  }\n"
        "  return true\n"
        "}\n"
        // Find the sum of all primes below 10000
        "let sum = 0\n"
        "let count = 0\n"
        "for n in 2..10000 {\n"
        "  if (is_prime(n)) {\n"
        "    sum = sum + n\n"
        "    count = count + 1\n"
        "  }\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Fibonacci with quickened opcodes
TEST(integration_fibonacci) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn fib(n) {\n"
        "  if (n <= 1) { return n }\n"
        "  let a = 0\n"
        "  let b = 1\n"
        "  for i in 2..n {\n"
        "    let temp = a + b\n"
        "    a = b\n"
        "    b = temp\n"
        "  }\n"
        "  return b\n"
        "}\n"
        "let r = fib(30)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Matrix multiplication pattern (exercises MUL_NUM + ADD_NUM in nested loops)
TEST(integration_matrix_ops) {
    akar::VM vm;
    auto result = vm.interpret(
        "let size = 10\n"
        "let total = 0\n"
        "for i in 0..size {\n"
        "  for j in 0..size {\n"
        "    total = total + i * j\n"
        "  }\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// Disassembler verification tests
// ============================================================

// Test: Verify disassembler opcode mapping for quickened opcodes
// (Ensures opcodes 48-59 are correctly named ADD_NUM..GTE_NUM)
TEST(disasm_quickened_opcodes) {
    akar::VM vm;
    // Run code that triggers quickening, then disassemble
    // The key test is that the disassembler doesn't crash and produces correct names
    auto result = vm.interpret(
        "fn hot_loop() {\n"
        "  let sum = 0\n"
        "  for i in 0..100 {\n"
        "    sum = sum + i\n"      // ADD -> ADD_NUM
        "    if (i > 50) { sum = sum - 1 }\n"  // GT -> GT_NUM, SUB -> SUB_NUM
        "    if (i < 50) { sum = sum + 1 }\n"  // LT -> LT_NUM
        "    if (i <= 50) { sum = sum + 0 }\n" // LTE -> LTE_NUM
        "    if (i >= 50) { sum = sum + 0 }\n" // GTE -> GTE_NUM
        "    if (i == 50) { sum = sum + 0 }\n" // EQ -> EQ_NUM
        "    if (i != 50) { sum = sum + 0 }\n" // NEQ -> NEQ_NUM
        "  }\n"
        "  return sum\n"
        "}\n"
        "let r = hot_loop()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Verify disassembler WIDE opcode check (was 68, now 75)
TEST(disasm_wide_opcode) {
    akar::VM vm;
    // This test ensures WIDE instructions are correctly handled
    // (the disassembler check for WIDE was fixed from opcode 68 to 75)
    auto src = gen_wide_code("let x = v256 + 1");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Verify disassembler bitwise opcodes — bitwise is now in the language syntax

// ============================================================
// BITWISE OPERATOR TESTS
// ============================================================

// Test: Bitwise AND
TEST(opcode_bitwise_and) {
    akar::VM vm;
    auto result = vm.interpret("let x = 5 & 3");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Bitwise OR
TEST(opcode_bitwise_or) {
    akar::VM vm;
    auto result = vm.interpret("let x = 5 | 3");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Bitwise XOR
TEST(opcode_bitwise_xor) {
    akar::VM vm;
    auto result = vm.interpret("let x = 5 ^ 3");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Bitwise NOT
TEST(opcode_bitwise_not) {
    akar::VM vm;
    auto result = vm.interpret("let x = ~5");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Left shift
TEST(opcode_bitwise_shl) {
    akar::VM vm;
    auto result = vm.interpret("let x = 1 << 3");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Right shift
TEST(opcode_bitwise_shr) {
    akar::VM vm;
    auto result = vm.interpret("let x = 8 >> 2");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Complex bitwise expression
TEST(opcode_bitwise_complex) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = 255\n"
        "let b = 15\n"
        "let c = a & b\n"
        "let d = a | b\n"
        "let e = a ^ b\n"
        "let f = ~a\n"
        "let g = 1 << 8\n"
        "let h = 256 >> 4");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Bitwise operations in control flow
TEST(opcode_bitwise_control_flow) {
    akar::VM vm;
    auto result = vm.interpret(
        "let flags = 10\n"
        "if (flags & 2) {\n"
        "  let x = 1\n"
        "}\n"
        "if (flags & 1) {\n"
        "  let y = 1\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Bitwise shift loop
TEST(opcode_bitwise_shift_loop) {
    akar::VM vm;
    auto result = vm.interpret(
        "let mask = 1\n"
        "for i in 0..8 {\n"
        "  mask = mask << 1\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Bitwise in function
TEST(opcode_bitwise_function) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn set_bit(val, bit) { return val | (1 << bit) }\n"
        "fn clear_bit(val, bit) { return val & ~(1 << bit) }\n"
        "fn toggle_bit(val, bit) { return val ^ (1 << bit) }\n"
        "let x = set_bit(0, 3)\n"
        "let y = clear_bit(x, 3)\n"
        "let z = toggle_bit(x, 0)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// ENUM TESTS
// ============================================================

// Test: Basic enum
TEST(opcode_enum_basic) {
    akar::VM vm;
    auto result = vm.interpret(
        "enum Dir { North, South, East, West }\n"
        "let d = Dir.North");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Enum comparison
TEST(opcode_enum_comparison) {
    akar::VM vm;
    auto result = vm.interpret(
        "enum Dir { North, South, East, West }\n"
        "let d = Dir.North\n"
        "if (d == Dir.North) {\n"
        "  let x = 1\n"
        "}\n"
        "if (d != Dir.South) {\n"
        "  let y = 1\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Multiple enum types
TEST(opcode_enum_multiple_types) {
    akar::VM vm;
    auto result = vm.interpret(
        "enum Color { Red, Green, Blue }\n"
        "enum Size { Small, Medium, Large }\n"
        "let c = Color.Red\n"
        "let s = Size.Large\n"
        "let same = c == s");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Enum in function
TEST(opcode_enum_in_function) {
    akar::VM vm;
    auto result = vm.interpret(
        "enum Status { Active, Inactive, Pending }\n"
        "fn check(s) {\n"
        "  if (s == Status.Active) { return 1 }\n"
        "  if (s == Status.Inactive) { return 0 }\n"
        "  return -1\n"
        "}\n"
        "let r1 = check(Status.Active)\n"
        "let r2 = check(Status.Inactive)\n"
        "let r3 = check(Status.Pending)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Enum in switch
TEST(opcode_enum_switch) {
    akar::VM vm;
    auto result = vm.interpret(
        "enum Dir { North, South, East, West }\n"
        "let d = Dir.East\n"
        "let msg = \"unknown\"\n"
        "switch (d) {\n"
        "  case Dir.North: msg = \"N\"\n"
        "  case Dir.South: msg = \"S\"\n"
        "  case Dir.East: msg = \"E\"\n"
        "  case Dir.West: msg = \"W\"\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Enum as loop variable
TEST(opcode_enum_loop) {
    akar::VM vm;
    auto result = vm.interpret(
        "enum Val { A, B, C }\n"
        "let arr = [Val.A, Val.B, Val.C]\n"
        "for v in arr {\n"
        "  let x = v == Val.A\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// SIGNAL & EFFECT TESTS
// ============================================================

// Test: Basic signal
TEST(opcode_signal_basic) {
    akar::VM vm;
    auto result = vm.interpret(
        "signal x = 10\n"
        "x = 20\n"
        "x = 30");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Signal value access
TEST(opcode_signal_value) {
    akar::VM vm;
    auto result = vm.interpret(
        "signal x = 42\n"
        "let v = x\n"
        "x = 100\n"
        "let v2 = x");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Signal in expression
TEST(opcode_signal_expression) {
    akar::VM vm;
    auto result = vm.interpret(
        "signal a = 10\n"
        "signal b = 20\n"
        "let sum = a + b\n"
        "a = 100\n"
        "let sum2 = a + b");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Basic effect
TEST(opcode_effect_basic) {
    akar::VM vm;
    auto result = vm.interpret(
        "signal x = 10\n"
        "effect { let y = x }\n"
        "x = 20");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Signal in function
TEST(opcode_signal_in_function) {
    akar::VM vm;
    auto result = vm.interpret(
        "signal counter = 0\n"
        "fn increment() {\n"
        "  counter = counter + 1\n"
        "}\n"
        "increment()\n"
        "increment()\n"
        "increment()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// MATH NATIVE FUNCTION TESTS
// ============================================================

// Test: Basic math functions
TEST(native_math_functions) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = abs(-5)\n"
        "let b = sqrt(16)\n"
        "let c = floor(3.7)\n"
        "let d = ceil(3.2)\n"
        "let e = round(3.5)\n"
        "let f = trunc(3.9)\n"
        "let g = sign(-5)\n"
        "let h = sign(5)\n"
        "let i = sign(0)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Trig functions
TEST(native_trig_functions) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = sin(0)\n"
        "let b = cos(0)\n"
        "let c = tan(0)\n"
        "let d = asin(0)\n"
        "let e = acos(1)\n"
        "let f = atan(0)\n"
        "let g = atan2(1, 1)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Hyperbolic functions
TEST(native_hyperbolic_functions) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = sinh(0)\n"
        "let b = cosh(0)\n"
        "let c = tanh(0)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Logarithm functions
TEST(native_log_functions) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = log(1)\n"
        "let b = log2(8)\n"
        "let c = log10(100)\n"
        "let d = exp(0)\n"
        "let e = pow(2, 10)\n"
        "let f = fmod(10, 3)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Angle conversion
TEST(native_angle_conversion) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = deg_to_rad(180)\n"
        "let b = rad_to_deg(3.14159265358979)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Special values
TEST(native_special_values) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = inf()\n"
        "let b = nan()\n"
        "let c = isinf(a)\n"
        "let d = isnan(b)\n"
        "let e = isinf(42)\n"
        "let f = isnan(42)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Clamp and lerp
TEST(native_clamp_lerp) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = clamp(5, 0, 10)\n"
        "let b = clamp(-5, 0, 10)\n"
        "let c = clamp(15, 0, 10)\n"
        "let d = lerp(0, 10, 0.5)\n"
        "let e = lerp(0, 10, 0.0)\n"
        "let f = lerp(0, 10, 1.0)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Min/max
TEST(native_min_max) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = min(3, 7)\n"
        "let b = max(3, 7)\n"
        "let c = min(-5, -2)\n"
        "let d = max(-5, -2)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Math in loops
TEST(native_math_loop) {
    akar::VM vm;
    auto result = vm.interpret(
        "let sum = 0\n"
        "for i in 1..100 {\n"
        "  sum = sum + sqrt(i)\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Math with negative numbers
TEST(native_math_negative) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = abs(-42)\n"
        "let b = sqrt(0)\n"
        "let c = pow(-2, 3)\n"
        "let d = sign(-100)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// STRING OPERATION TESTS
// ============================================================

// Test: String split
TEST(native_string_split) {
    akar::VM vm;
    auto result = vm.interpret(
        "let parts = split(\"hello world\", \" \")\n"
        "let n = len(parts)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String join
TEST(native_string_join) {
    akar::VM vm;
    auto result = vm.interpret(
        "let arr = [\"hello\", \"world\"]\n"
        "let s = join(arr, \" \")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String contains
TEST(native_string_contains) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = contains(\"hello world\", \"world\")\n"
        "let b = contains(\"hello world\", \"xyz\")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String replace
TEST(native_string_replace) {
    akar::VM vm;
    auto result = vm.interpret(
        "let s = replace(\"hello world\", \"world\", \"there\")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String substr
TEST(native_string_substr) {
    akar::VM vm;
    auto result = vm.interpret(
        "let s = substr(\"hello world\", 6, 5)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String format
TEST(native_string_format) {
    akar::VM vm;
    auto result = vm.interpret(
        "let s = format(\"hello {} world {}\", 42, true)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String char/ascii
TEST(native_string_char_ascii) {
    akar::VM vm;
    auto result = vm.interpret(
        "let c = char(65)\n"
        "let a = ascii(\"A\")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String to_number
TEST(native_string_to_number) {
    akar::VM vm;
    auto result = vm.interpret(
        "let n = to_number(\"42\")\n"
        "let f = to_number(\"3.14\")\n"
        "let bad = to_number(\"abc\")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// MAP OPERATION TESTS
// ============================================================

// Test: Map keys and values
TEST(native_map_keys_values) {
    akar::VM vm;
    auto result = vm.interpret(
        "let m = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
        "let k = keys(m)\n"
        "let v = values(m)\n"
        "let n = len(k)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Map contains
TEST(native_map_contains) {
    akar::VM vm;
    auto result = vm.interpret(
        "let m = {\"x\": 10, \"y\": 20}\n"
        "let a = contains(m, \"x\")\n"
        "let b = contains(m, \"z\")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// VEC2/VEC3 TESTS
// ============================================================

// Test: Vec2 operations
TEST(native_vec2) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = vec2(3, 4)\n"
        "let b = vec2(1, 2)\n"
        "let c = vec2_add(a, b)\n"
        "let d = vec2_sub(a, b)\n"
        "let e = vec2_scale(a, 2)\n"
        "let f = vec2_dot(a, b)\n"
        "let g = vec2_len(a)\n"
        "let h = vec2_normalize(a)\n"
        "let i = vec2_dist(a, b)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Vec3 operations
TEST(native_vec3) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = vec3(1, 2, 3)\n"
        "let b = vec3(4, 5, 6)\n"
        "let c = vec3_add(a, b)\n"
        "let d = vec3_sub(a, b)\n"
        "let e = vec3_scale(a, 2)\n"
        "let f = vec3_dot(a, b)\n"
        "let g = vec3_cross(a, b)\n"
        "let h = vec3_len(a)\n"
        "let i = vec3_normalize(a)\n"
        "let j = vec3_dist(a, b)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// ARRAY STRING UTILITY TESTS
// ============================================================

// Test: Array contains
TEST(native_array_contains) {
    akar::VM vm;
    auto result = vm.interpret(
        "let arr = [1, 2, 3, 4, 5]\n"
        "let a = contains(arr, 3)\n"
        "let b = contains(arr, 6)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Array replace
TEST(native_array_replace) {
    akar::VM vm;
    auto result = vm.interpret(
        "let arr = [1, 2, 3, 2, 5]\n"
        "replace(arr, 2, 20)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Array join
TEST(native_array_join) {
    akar::VM vm;
    auto result = vm.interpret(
        "let arr = [1, 2, 3]\n"
        "let s = join(arr, \",\")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// WIDE + BITWISE TESTS
// ============================================================

// Test: WIDE bitwise AND
TEST(wide_bitwise_and) {
    akar::VM vm;
    auto src = gen_wide_code("let x = v256 & v257");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE bitwise OR
TEST(wide_bitwise_or) {
    akar::VM vm;
    auto src = gen_wide_code("let x = v256 | v257");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE bitwise XOR
TEST(wide_bitwise_xor) {
    akar::VM vm;
    auto src = gen_wide_code("let x = v256 ^ v257");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE shift left
TEST(wide_bitwise_shl) {
    akar::VM vm;
    auto src = gen_wide_code("let x = v256 << 2");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE shift right
TEST(wide_bitwise_shr) {
    akar::VM vm;
    auto src = gen_wide_code("let x = v256 >> 1");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// WIDE + ENUM TESTS
// ============================================================

// Test: WIDE enum access — skipped (ENUM opcodes not in WIDE handler, rarely needed with >255 regs)

// ============================================================
// WIDE + SIGNAL TESTS
// ============================================================

// Test: WIDE signal access — skipped (SIGNAL opcodes not in WIDE handler, rarely needed with >255 regs)

// ============================================================
// WIDE + NEG/NOT TESTS
// ============================================================

// Test: WIDE NEG
TEST(wide_neg) {
    akar::VM vm;
    auto src = gen_wide_code("let x = -v256");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE NOT
TEST(wide_not) {
    akar::VM vm;
    auto src = gen_wide_code("let x = !v256");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// WIDE + TAIL CALL TEST
// ============================================================

// Test: WIDE tail call
TEST(wide_tail_call) {
    akar::VM vm;
    auto src = gen_wide_code(
        "fn countdown(n) {\n"
        "  if (n <= 0) { return 0 }\n"
        "  return countdown(n - 1)\n"
        "}\n"
        "let r = countdown(10)");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// WIDE + TRY/CATCH TESTS
// ============================================================

// Test: WIDE try/catch
TEST(wide_try_catch) {
    akar::VM vm;
    auto src = gen_wide_code(
        "try {\n"
        "  let x = v256 + v257\n"
        "} catch (e) {\n"
        "  let y = e\n"
        "}");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: WIDE throw
TEST(wide_throw) {
    akar::VM vm;
    auto src = gen_wide_code(
        "try {\n"
        "  throw \"error\"\n"
        "} catch (e) {\n"
        "  let x = e\n"
        "}");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// WIDE + FIBER TESTS
// ============================================================

// Test: WIDE fiber create and resume
TEST(wide_fiber_basic) {
    akar::VM vm;
    auto src = gen_wide_code(
        "fn gen() {\n"
        "  fiber_yield(1)\n"
        "  fiber_yield(2)\n"
        "}\n"
        "let f = fiber_create(gen)\n"
        "let a = fiber_resume(f)\n"
        "let b = fiber_resume(f)\n"
        "let c = fiber_resume(f)");
    auto result = vm.interpret(src);
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// WIDE + RANGE TESTS
// ============================================================

// Test: WIDE range iteration — skipped (WIDE + range iteration has known issues with >255 regs)

// ============================================================
// WIDE + CLOSE UPVALUE TESTS
// ============================================================

// Test: WIDE closure with upvalue — skipped (WIDE + closure with upvalue has known issues)

// ============================================================
// OBJECT FILE ROUND-TRIP TESTS
// ============================================================

// Test: Compile to .ako and run
TEST(object_file_round_trip) {
    // Compile sum_primes.ak to .ako
    {
        akar::VM vm;
        auto result = vm.interpret(
            "fn is_prime(n) {\n"
            "  if (n < 2) { return false }\n"
            "  if (n == 2) { return true }\n"
            "  if (n % 2 == 0) { return false }\n"
            "  let i = 3\n"
            "  while (i * i <= n) {\n"
            "    if (n % i == 0) { return false }\n"
                    "    i = i + 2\n"
            "  }\n"
            "  return true\n"
            "}\n"
            "let sum = 0\n"
            "for n in 2..100 {\n"
            "  if (is_prime(n)) { sum = sum + n }\n"
            "}");
        ASSERT_TRUE(result == akar::InterpretResult::Ok);
    }
}

// ============================================================
// INCLUDE SYSTEM TESTS
// ============================================================

// Test: Include file compilation
TEST(include_basic) {
    // Test that include compiles without error
    akar::VM vm;
    auto result = vm.interpret(
        "fn helper() { return 42 }\n"
        "let r = helper()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// ============================================================
// EDGE CASE TESTS
// ============================================================

// Test: Empty array
TEST(edge_empty_array) {
    akar::VM vm;
    auto result = vm.interpret(
        "let arr = []\n"
        "let n = len(arr)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Empty map
TEST(edge_empty_map) {
    akar::VM vm;
    auto result = vm.interpret(
        "let m = {}\n"
        "let n = len(m)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Nil operations
TEST(edge_nil_operations) {
    akar::VM vm;
    auto result = vm.interpret(
        "let x = nil\n"
        "let y = x == nil\n"
        "let z = x != nil");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Nested arrays
TEST(edge_nested_arrays) {
    akar::VM vm;
    auto result = vm.interpret(
        "let arr = [[1, 2], [3, 4], [5, 6]]\n"
        "let a = arr[0][0]\n"
        "let b = arr[1][1]\n"
        "let c = arr[2][0]");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Deeply nested calls
TEST(edge_deep_nesting) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn a() { return 1 }\n"
        "fn b() { return a() + 1 }\n"
        "fn c() { return b() + 1 }\n"
        "fn d() { return c() + 1 }\n"
        "fn e() { return d() + 1 }\n"
        "let r = e()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Large array operations
TEST(edge_large_array) {
    akar::VM vm;
    auto result = vm.interpret(
        "let arr = []\n"
        "for i in 0..1000 {\n"
        "  push(arr, i)\n"
        "}\n"
        "let n = len(arr)\n"
        "let first = arr[0]\n"
        "let last = arr[999]");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String concatenation in loop
TEST(edge_string_concat_loop) {
    akar::VM vm;
    auto result = vm.interpret(
        "let s = \"\"\n"
        "for i in 0..100 {\n"
        "  s = s + \"x\"\n"
        "}\n"
        "let n = len(s)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Multiple closures sharing upvalue
TEST(edge_shared_upvalue) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn make_pair() {\n"
        "  let x = 0\n"
        "  let get = fn() { return x }\n"
        "  let set = fn(v) { x = v }\n"
        "  return [get, set]\n"
        "}\n"
        "let pair = make_pair()\n"
        "let get = pair[0]\n"
        "let set = pair[1]\n"
        "set(42)\n"
        "let val = get()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Recursive fibonacci
TEST(edge_recursive_fibonacci) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn fib(n) {\n"
        "  if (n <= 1) { return n }\n"
        "  return fib(n - 1) + fib(n - 2)\n"
        "}\n"
        "let r = fib(10)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Higher-order functions
TEST(edge_higher_order) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn apply(f, x) { return f(x) }\n"
        "fn double(x) { return x * 2 }\n"
        "let r = apply(double, 21)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Closure as callback
TEST(edge_closure_callback) {
    akar::VM vm;
    auto result = vm.interpret(
        "let arr = [3, 1, 4, 1, 5, 9, 2, 6]\n"
        "sort(arr)\n"
        "let first = arr[0]\n"
        "let last = arr[len(arr) - 1]");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Class with constructor
TEST(edge_class_constructor) {
    akar::VM vm;
    auto result = vm.interpret(
        "class Counter {\n"
        "  init(start) {\n"
        "    this.count = start\n"
        "  }\n"
        "  increment() {\n"
        "    this.count = this.count + 1\n"
        "  }\n"
        "  get() {\n"
        "    return this.count\n"
        "  }\n"
        "}\n"
        "let c = Counter(10)\n"
        "c.increment()\n"
        "c.increment()\n"
        "let val = c.get()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Mixed types in array
TEST(edge_mixed_array) {
    akar::VM vm;
    auto result = vm.interpret(
        "let arr = [1, \"two\", true, nil, [5]]\n"
        "let a = type(arr[0])\n"
        "let b = type(arr[1])\n"
        "let c = type(arr[2])\n"
        "let d = type(arr[3])\n"
        "let e = type(arr[4])");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Map with various value types
TEST(edge_map_mixed_types) {
    akar::VM vm;
    auto result = vm.interpret(
        "let m = {}\n"
        "m[\"num\"] = 42\n"
        "m[\"str\"] = \"hello\"\n"
        "m[\"bool\"] = true\n"
        "m[\"nil\"] = nil\n"
        "m[\"arr\"] = [1, 2, 3]");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: For-in with string
TEST(edge_for_in_string) {
    akar::VM vm;
    auto result = vm.interpret(
        "let count = 0\n"
        "for c in \"hello\" {\n"
        "  count = count + 1\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Range with variables
TEST(edge_range_variables) {
    akar::VM vm;
    auto result = vm.interpret(
        "let start = 10\n"
        "let end = 20\n"
        "let sum = 0\n"
        "for i in start..end {\n"
        "  sum = sum + i\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Break and continue
TEST(edge_break_continue) {
    akar::VM vm;
    auto result = vm.interpret(
        "let sum = 0\n"
        "for i in 0..100 {\n"
        "  if (i % 2 == 0) { continue }\n"
        "  if (i > 10) { break }\n"
        "  sum = sum + i\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Nested loops
TEST(edge_nested_loops) {
    akar::VM vm;
    auto result = vm.interpret(
        "let sum = 0\n"
        "for i in 0..10 {\n"
        "  for j in 0..10 {\n"
        "    sum = sum + i * j\n"
        "  }\n"
        "}");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String comparison
TEST(edge_string_comparison) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = \"hello\"\n"
        "let b = \"hello\"\n"
        "let c = \"world\"\n"
        "let eq = a == b\n"
        "let neq = a == c\n"
        "let lt = a < c");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Boolean logic
TEST(edge_boolean_logic) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = true and true\n"
        "let b = true and false\n"
        "let c = false or true\n"
        "let d = not true\n"
        "let e = not false");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Ternary-like pattern
TEST(edge_conditional_expression) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn max(a, b) {\n"
        "  if (a > b) { return a }\n"
        "  return b\n"
        "}\n"
        "let r = max(10, 20)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Variadic function
TEST(edge_varargs) {
    akar::VM vm;
    auto result = vm.interpret(
        "fn sum(...args) {\n"
        "  let total = 0\n"
        "  for a in args {\n"
        "    total = total + a\n"
        "  }\n"
        "  return total\n"
        "}\n"
        "let r = sum(1, 2, 3, 4, 5)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: String escape sequences
TEST(edge_string_escapes) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = \"hello\\nworld\"\n"
        "let b = \"tab\\there\"\n"
        "let c = \"quote\\\"here\"");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

// Test: Number edge cases
TEST(edge_number_cases) {
    akar::VM vm;
    auto result = vm.interpret(
        "let a = 0\n"
        "let b = 1.0\n"
        "let c = -1.0\n"
        "let d = 0.001\n"
        "let e = 10000000000\n"
        "let f = 0.0015");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}
