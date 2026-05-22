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
// ---- Array Utility Tests ----

TEST(native_insert) {
    akar::VM vm;
    vm.interpret("let arr = [1, 2, 3]");
    auto* arr = vm.get_global("arr").as_array();
    ASSERT_TRUE(arr != nullptr);
    auto* insert_fn = vm.get_global("insert").as_native();
    ASSERT_TRUE(insert_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr"), akar::Value(1.0), akar::Value(99.0) };
    auto result = insert_fn->function(3, args);
    ASSERT_TRUE(result.is_array());
    ASSERT_EQ((int)arr->elements.size(), 4);
    ASSERT_EQ(arr->elements[1].get_number(), 99.0);
}

TEST(native_remove) {
    akar::VM vm;
    vm.interpret("let arr = [10, 20, 30]");
    auto* arr = vm.get_global("arr").as_array();
    auto* remove_fn = vm.get_global("remove").as_native();
    ASSERT_TRUE(remove_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr"), akar::Value(1.0) };
    auto result = remove_fn->function(2, args);
    ASSERT_TRUE(result.is_number());
    ASSERT_EQ(result.get_number(), 20.0);
    ASSERT_EQ((int)arr->elements.size(), 2);
    ASSERT_EQ(arr->elements[0].get_number(), 10.0);
    ASSERT_EQ(arr->elements[1].get_number(), 30.0);
}

TEST(native_shift) {
    akar::VM vm;
    vm.interpret("let arr = [1, 2, 3]");
    auto* arr = vm.get_global("arr").as_array();
    auto* shift_fn = vm.get_global("shift").as_native();
    ASSERT_TRUE(shift_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr") };
    auto result = shift_fn->function(1, args);
    ASSERT_TRUE(result.is_number());
    ASSERT_EQ(result.get_number(), 1.0);
    ASSERT_EQ((int)arr->elements.size(), 2);
    ASSERT_EQ(arr->elements[0].get_number(), 2.0);
}

TEST(native_unshift) {
    akar::VM vm;
    vm.interpret("let arr = [2, 3]");
    auto* arr = vm.get_global("arr").as_array();
    auto* unshift_fn = vm.get_global("unshift").as_native();
    ASSERT_TRUE(unshift_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr"), akar::Value(1.0) };
    auto result = unshift_fn->function(2, args);
    ASSERT_TRUE(result.is_array());
    ASSERT_EQ((int)arr->elements.size(), 3);
    ASSERT_EQ(arr->elements[0].get_number(), 1.0);
    ASSERT_EQ(arr->elements[1].get_number(), 2.0);
}

TEST(native_index_of) {
    akar::VM vm;
    vm.interpret("let arr = [10, 20, 30]");
    auto* index_of_fn = vm.get_global("index_of").as_native();
    ASSERT_TRUE(index_of_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr"), akar::Value(20.0) };
    auto result = index_of_fn->function(2, args);
    ASSERT_TRUE(result.is_number());
    ASSERT_EQ(result.get_number(), 1.0);
    akar::Value args2[] = { vm.get_global("arr"), akar::Value(99.0) };
    auto result2 = index_of_fn->function(2, args2);
    ASSERT_EQ(result2.get_number(), -1.0);
}

TEST(native_slice) {
    akar::VM vm;
    vm.interpret("let arr = [10, 20, 30, 40, 50]");
    auto* slice_fn = vm.get_global("slice").as_native();
    ASSERT_TRUE(slice_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr"), akar::Value(1.0), akar::Value(4.0) };
    auto result = slice_fn->function(3, args);
    ASSERT_TRUE(result.is_array());
    auto* sliced = result.as_array();
    ASSERT_EQ((int)sliced->elements.size(), 3);
    ASSERT_EQ(sliced->elements[0].get_number(), 20.0);
    ASSERT_EQ(sliced->elements[1].get_number(), 30.0);
    ASSERT_EQ(sliced->elements[2].get_number(), 40.0);
}

TEST(native_reverse) {
    akar::VM vm;
    vm.interpret("let arr = [1, 2, 3]");
    auto* arr = vm.get_global("arr").as_array();
    auto* reverse_fn = vm.get_global("reverse").as_native();
    ASSERT_TRUE(reverse_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr") };
    reverse_fn->function(1, args);
    ASSERT_EQ(arr->elements[0].get_number(), 3.0);
    ASSERT_EQ(arr->elements[1].get_number(), 2.0);
    ASSERT_EQ(arr->elements[2].get_number(), 1.0);
}

TEST(native_sort) {
    akar::VM vm;
    vm.interpret("let arr = [3, 1, 2]");
    auto* arr = vm.get_global("arr").as_array();
    auto* sort_fn = vm.get_global("sort").as_native();
    ASSERT_TRUE(sort_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr") };
    sort_fn->function(1, args);
    ASSERT_EQ(arr->elements[0].get_number(), 1.0);
    ASSERT_EQ(arr->elements[1].get_number(), 2.0);
    ASSERT_EQ(arr->elements[2].get_number(), 3.0);
}

TEST(native_fill) {
    akar::VM vm;
    auto* fill_fn = vm.get_global("fill").as_native();
    ASSERT_TRUE(fill_fn != nullptr);
    akar::Value args[] = { akar::Value(42.0), akar::Value(5.0) };
    auto result = fill_fn->function(2, args);
    ASSERT_TRUE(result.is_array());
    auto* arr = result.as_array();
    ASSERT_EQ((int)arr->elements.size(), 5);
    for (auto& e : arr->elements) {
        ASSERT_EQ(e.get_number(), 42.0);
    }
}

TEST(native_clear) {
    akar::VM vm;
    vm.interpret("let arr = [1, 2, 3]");
    auto* arr = vm.get_global("arr").as_array();
    auto* clear_fn = vm.get_global("clear").as_native();
    ASSERT_TRUE(clear_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr") };
    clear_fn->function(1, args);
    ASSERT_TRUE(arr->elements.empty());
}

TEST(native_flatten) {
    akar::VM vm;
    vm.interpret("let arr = [[1, 2], 3, [4, 5]]");
    auto* flatten_fn = vm.get_global("flatten").as_native();
    ASSERT_TRUE(flatten_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr") };
    auto result = flatten_fn->function(1, args);
    ASSERT_TRUE(result.is_array());
    auto* flat = result.as_array();
    ASSERT_EQ((int)flat->elements.size(), 5);
    ASSERT_EQ(flat->elements[0].get_number(), 1.0);
    ASSERT_EQ(flat->elements[2].get_number(), 3.0);
    ASSERT_EQ(flat->elements[4].get_number(), 5.0);
}

TEST(native_unique) {
    akar::VM vm;
    vm.interpret("let arr = [1, 2, 2, 3, 1, 4]");
    auto* unique_fn = vm.get_global("unique").as_native();
    ASSERT_TRUE(unique_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr") };
    auto result = unique_fn->function(1, args);
    ASSERT_TRUE(result.is_array());
    auto* uniq = result.as_array();
    ASSERT_EQ((int)uniq->elements.size(), 4);
    ASSERT_EQ(uniq->elements[0].get_number(), 1.0);
    ASSERT_EQ(uniq->elements[1].get_number(), 2.0);
    ASSERT_EQ(uniq->elements[2].get_number(), 3.0);
    ASSERT_EQ(uniq->elements[3].get_number(), 4.0);
}

TEST(native_concat_arrays) {
    akar::VM vm;
    vm.interpret("let a = [1, 2]\nlet b = [3, 4]");
    auto* concat_fn = vm.get_global("concat_arrays").as_native();
    ASSERT_TRUE(concat_fn != nullptr);
    akar::Value args[] = { vm.get_global("a"), vm.get_global("b") };
    auto result = concat_fn->function(2, args);
    ASSERT_TRUE(result.is_array());
    auto* arr = result.as_array();
    ASSERT_EQ((int)arr->elements.size(), 4);
    ASSERT_EQ(arr->elements[0].get_number(), 1.0);
    ASSERT_EQ(arr->elements[3].get_number(), 4.0);
}

TEST(native_every) {
    akar::VM vm;
    vm.interpret("let arr = [2, 4, 6]\nfn is_even(x) { return x % 2 == 0 }");
    auto* every_fn = vm.get_global("every").as_native();
    ASSERT_TRUE(every_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr"), vm.get_global("is_even") };
    auto result = every_fn->function(2, args);
    ASSERT_TRUE(result.is_truthy());
}

TEST(native_some) {
    akar::VM vm;
    vm.interpret("let arr = [1, 3, 4]\nfn is_even(x) { return x % 2 == 0 }");
    auto* some_fn = vm.get_global("some").as_native();
    ASSERT_TRUE(some_fn != nullptr);
    akar::Value args[] = { vm.get_global("arr"), vm.get_global("is_even") };
    auto result = some_fn->function(2, args);
    ASSERT_TRUE(result.is_truthy());
}
