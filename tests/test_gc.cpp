#include "akar/vm/vm.h"
#include "akar/common/value.h"
#include <cmath>

using namespace akar;

// ============================================================
// GC Unit Tests — tri-color mark-sweep garbage collector
// ============================================================

static void reset_gc_state() {
    free_all_objects();
    reset_allocated_bytes();
    set_next_gc(1024 * 1024);
}

// --- Allocation Tracking ---

TEST(gc_allocation_tracking) {
    reset_gc_state();
    size_t before = get_allocated_bytes();
    allocate_array();
    ASSERT_TRUE(get_allocated_bytes() > before);
    allocate_map();
    allocate_string("hello world");
    ASSERT_TRUE(get_allocated_bytes() > before);
}

TEST(gc_alloc_accumulates) {
    reset_gc_state();
    size_t before = get_allocated_bytes();
    for (int i = 0; i < 100; i++) allocate_array();
    ASSERT_TRUE(get_allocated_bytes() >= before + 100 * sizeof(ObjArray));
}

// --- Full Collection ---

TEST(gc_full_sweep_unreachable) {
    reset_gc_state();
    akar::VM vm;
    size_t baseline = get_allocated_bytes();
    for (int i = 0; i < 50; i++) {
        allocate_array();
        allocate_map();
        allocate_string("orphan_" + std::to_string(i));
    }
    size_t with_orphans = get_allocated_bytes();
    ASSERT_TRUE(with_orphans > baseline);
    vm.collect_garbage();
    ASSERT_TRUE(get_allocated_bytes() < with_orphans);
}

TEST(gc_preserves_stack_roots) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "let arr = [1, 2, 3, 4, 5]\n"
        "let s = \"hello world\"\n"
        "let m = {\"a\": 1, \"b\": 2}") == InterpretResult::Ok);
    vm.collect_garbage();
    auto a = vm.get_global("arr");
    ASSERT_TRUE(a.is_array());
    ASSERT_EQ((int)a.as_array()->elements.size(), 5);
    auto s = vm.get_global("s");
    ASSERT_TRUE(s.is_string());
    ASSERT_TRUE(s.as_string()->value == "hello world");
    ASSERT_TRUE(vm.get_global("m").is_map());
}

TEST(gc_preserves_globals) {
    reset_gc_state();
    akar::VM vm;
    vm.set_global("mystr", Value(allocate_string("persistent")));
    vm.set_global("myarr", Value(allocate_array()));
    vm.collect_garbage();
    auto s = vm.get_global("mystr");
    ASSERT_TRUE(s.is_string());
    ASSERT_TRUE(s.as_string()->value == "persistent");
    ASSERT_TRUE(vm.get_global("myarr").is_array());
}

TEST(gc_preserves_closures) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "let base = 100\n"
        "fn make_adder(x) { fn adder(y) { return x + y + base } return adder }\n"
        "let add5 = make_adder(5)\n"
        "let r1 = add5(3)") == InterpretResult::Ok);
    ASSERT_EQ(vm.get_global("r1").get_number(), 108.0);
    vm.collect_garbage();
    ASSERT_TRUE(vm.get_global("add5").is_closure());
    ASSERT_EQ(vm.get_global("r1").get_number(), 108.0);
    ASSERT_TRUE(vm.get_global("make_adder").is_closure());
}

TEST(gc_preserves_nested_refs) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "let inner = {\"key\": \"deep value\"}\n"
        "let arr = [inner, 42, \"hello\"]\n"
        "let outer = {\"arr\": arr}") == InterpretResult::Ok);
    vm.collect_garbage();
    auto outer = vm.get_global("outer");
    ASSERT_TRUE(outer.is_map());
    auto it = outer.as_map()->entries.find("arr");
    ASSERT_TRUE(it != outer.as_map()->entries.end());
    ASSERT_TRUE(it->second.is_array());
    ASSERT_EQ((int)it->second.as_array()->elements.size(), 3);
    ASSERT_TRUE(it->second.as_array()->elements[0].is_map());
}

// --- Incremental GC ---

TEST(gc_incremental_preserves_roots) {
    reset_gc_state();
    akar::VM vm;
    vm.interpret(
        "let important = [1, 2, 3]\n"
        "let name = \"survivor\"\n"
        "let data = {\"x\": 100}");
    set_next_gc(0);
    for (int i = 0; i < 500; i++) {
        vm.gc_step();
        if (vm.gc_phase_ == VM::GCPhase::Idle) break;
    }
    ASSERT_TRUE(vm.get_global("important").is_array());
    ASSERT_TRUE(vm.get_global("name").is_string());
    ASSERT_TRUE(vm.get_global("name").as_string()->value == "survivor");
    ASSERT_TRUE(vm.get_global("data").is_map());
}

// --- Threshold ---

TEST(gc_threshold_triggers) {
    reset_gc_state();
    akar::VM vm;
    size_t initial = get_next_gc();
    ASSERT_TRUE(initial > 0);
    vm.interpret("let arr = [1,2,3,4,5]");
    ASSERT_TRUE(get_next_gc() >= initial);
}

// --- String Table ---

TEST(gc_string_table_survives) {
    reset_gc_state();
    akar::VM vm;
    vm.interpret("let s1 = \"interned1\"\nlet s2 = \"interned2\"\nlet s3 = s1 + s2");
    vm.collect_garbage();
    ASSERT_TRUE(vm.get_global("s1").is_string());
    ASSERT_TRUE(vm.get_global("s1").as_string()->value == "interned1");
    ASSERT_TRUE(vm.interpret("let s4 = \"interned1\"") == InterpretResult::Ok);
    ASSERT_TRUE(vm.get_global("s4").as_string()->value == "interned1");
}

// --- Object Type Preservation ---

TEST(gc_preserves_class_instances) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "class Point {\n"
        "  init(x, y) { this.x = x this.y = y }\n"
        "  dist() { return this.x * this.x + this.y * this.y }\n"
        "}\n"
        "let p = Point(3, 4)\n"
        "let d = p.dist()") == InterpretResult::Ok);
    ASSERT_EQ(vm.get_global("d").get_number(), 25.0);
    vm.collect_garbage();
    ASSERT_TRUE(vm.interpret("let p2 = Point(5, 12)\nlet d2 = p2.dist()") == InterpretResult::Ok);
    ASSERT_EQ(vm.get_global("d2").get_number(), 169.0);
}

TEST(gc_preserves_fibers) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "fn gen() { fiber_yield(10) fiber_yield(20) return 30 }\n"
        "let f = fiber_create(gen)\n"
        "let v1 = fiber_resume(f, nil)") == InterpretResult::Ok);
    ASSERT_EQ(vm.get_global("v1").get_number(), 10.0);
    vm.collect_garbage();
    ASSERT_TRUE(vm.interpret(
        "let f2 = fiber_create(gen)\n"
        "let v2 = fiber_resume(f2, nil)") == InterpretResult::Ok);
    ASSERT_EQ(vm.get_global("v2").get_number(), 10.0);
}

TEST(gc_preserves_signals) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "signal counter = 0\ncounter = 42\nlet v = counter") == InterpretResult::Ok);
    ASSERT_EQ(vm.get_global("v").get_number(), 42.0);
    vm.collect_garbage();
    ASSERT_TRUE(vm.interpret("counter = 100\nlet v2 = counter") == InterpretResult::Ok);
    ASSERT_EQ(vm.get_global("v2").get_number(), 100.0);
}

TEST(gc_preserves_effects) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "signal x = 1\nlet log = []\neffect { push(log, x) }\nx = 2") == InterpretResult::Ok);
    vm.collect_garbage();
    ASSERT_TRUE(vm.get_global("log").is_array());
}

// --- Multi-VM ---

TEST(gc_multi_vm_roots) {
    reset_gc_state();
    akar::VM vm1, vm2;
    vm1.interpret("let data1 = [1, 2, 3]\nlet name1 = \"vm1\"");
    vm2.interpret("let data2 = [4, 5, 6]\nlet name2 = \"vm2\"");
    vm1.collect_garbage();
    ASSERT_TRUE(vm1.get_global("data1").is_array());
    ASSERT_EQ((int)vm1.get_global("data1").as_array()->elements.size(), 3);
    ASSERT_TRUE(vm2.get_global("data2").is_array());
    ASSERT_EQ((int)vm2.get_global("data2").as_array()->elements.size(), 3);
    ASSERT_TRUE(vm1.get_global("name1").as_string()->value == "vm1");
    ASSERT_TRUE(vm2.get_global("name2").as_string()->value == "vm2");
}

// --- Write Barrier ---

TEST(gc_write_barrier) {
    reset_gc_state();
    akar::VM vm;
    vm.interpret("let arr = [1,2,3,4,5]");
    set_next_gc(0);
    vm.gc_step();
    allocate_string("during GC");
    allocate_array();
    for (int i = 0; i < 500; i++) {
        vm.gc_step();
        if (vm.gc_phase_ == VM::GCPhase::Idle) break;
    }
    ASSERT_TRUE(vm.interpret("let x = 42") == InterpretResult::Ok);
    ASSERT_EQ(vm.get_global("x").get_number(), 42.0);
}

// --- Edge Cases ---

TEST(gc_empty_vm) {
    reset_gc_state();
    akar::VM vm;
    size_t before = get_allocated_bytes();
    vm.collect_garbage();
    ASSERT_TRUE(get_allocated_bytes() <= before + 100);
}

TEST(gc_only_primitives) {
    reset_gc_state();
    akar::VM vm;
    vm.interpret("let a = 42\nlet b = 3.14\nlet c = true\nlet d = nil");
    size_t before = get_allocated_bytes();
    vm.collect_garbage();
    ASSERT_TRUE(get_allocated_bytes() <= before + 100);
}

TEST(gc_nested_closures) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "let x = 1\n"
        "fn a() { let y = 2\n"
        "  fn b() { let z = 3\n"
        "    fn c() { return x + y + z }\n"
        "    return c }\n"
        "  return b }\n"
        "let f = a()()\n"
        "let r = f()") == InterpretResult::Ok);
    ASSERT_EQ(vm.get_global("r").get_number(), 6.0);
    vm.collect_garbage();
    ASSERT_TRUE(vm.get_global("f").is_closure());
    ASSERT_EQ(vm.get_global("r").get_number(), 6.0);
}

TEST(gc_try_catch_roots) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "let data = [1, 2, 3]\n"
        "try { throw \"error\" } catch (e) { let x = data }\n"
        "let f = data") == InterpretResult::Ok);
    vm.collect_garbage();
    ASSERT_TRUE(vm.get_global("data").is_array());
    ASSERT_EQ((int)vm.get_global("data").as_array()->elements.size(), 3);
}

TEST(gc_preserves_iterators) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "fn sum_array(a) { let s = [0] for x in a { s[0] = s[0] + x } return s[0] }\n"
        "let answer = sum_array([10, 20, 30, 40, 50])") == InterpretResult::Ok);
    ASSERT_TRUE(vm.get_global("answer").is_number());
    ASSERT_EQ(vm.get_global("answer").get_number(), 150.0);
}

TEST(gc_allocation_burst) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "fn burst() { let last = nil for i in 0..1000 { last = [i, i+1, i+2] } return last }\n"
        "let answer = burst()") == InterpretResult::Ok);
    ASSERT_TRUE(vm.get_global("answer").is_array());
    ASSERT_EQ((int)vm.get_global("answer").as_array()->elements.size(), 3);
}

TEST(gc_map_heavy) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "let m = {\"a\": 1, \"b\": 2, \"c\": 3, \"d\": 4, \"e\": 5}") == InterpretResult::Ok);
    vm.collect_garbage();
    ASSERT_TRUE(vm.get_global("m").is_map());
    ASSERT_EQ((int)vm.get_global("m").as_map()->entries.size(), 5);
}

TEST(gc_closure_globals) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "let captured = [10, 20, 30]\n"
        "fn get_captured() { return captured }\n"
        "fn set_captured(v) { captured = v }") == InterpretResult::Ok);
    vm.collect_garbage();
    auto c = vm.get_global("captured");
    ASSERT_TRUE(c.is_array());
    ASSERT_EQ((int)c.as_array()->elements.size(), 3);
    ASSERT_EQ(c.as_array()->elements[0].get_number(), 10.0);
}

TEST(gc_mixed_object_types) {
    reset_gc_state();
    akar::VM vm;
    ASSERT_TRUE(vm.interpret(
        "let arr = [1, 2, 3]\n"
        "let m = {\"x\": 10}\n"
        "let s = \"hello\"\n"
        "class Foo { init(v) { this.v = v } }\n"
        "let obj = Foo(42)\n"
        "fn maker() { let inner = [100] return inner }\n"
        "let cr = maker()") == InterpretResult::Ok);
    vm.collect_garbage();
    ASSERT_TRUE(vm.get_global("arr").is_array());
    ASSERT_TRUE(vm.get_global("m").is_map());
    ASSERT_TRUE(vm.get_global("s").is_string());
    ASSERT_TRUE(vm.get_global("obj").is_instance());
    ASSERT_TRUE(vm.get_global("cr").is_array());
}

TEST(gc_post_gc_vm_works) {
    reset_gc_state();
    akar::VM vm;
    vm.collect_garbage();
    set_next_gc(1024 * 1024);
    ASSERT_TRUE(vm.interpret("let arr = [0, 2, 4, 6, 8]") == InterpretResult::Ok);
    ASSERT_TRUE(vm.get_global("arr").is_array());
    ASSERT_EQ((int)vm.get_global("arr").as_array()->elements.size(), 5);
}

TEST(gc_post_collection_allocation) {
    reset_gc_state();
    akar::VM vm;
    vm.collect_garbage();
    set_next_gc(1024 * 1024);
    ASSERT_TRUE(vm.interpret(
        "let arr = [1, 2, 3]\n"
        "let m = {\"key\": \"value\"}\n"
        "let s = \"hello\" + \"world\"") == InterpretResult::Ok);
    ASSERT_TRUE(vm.get_global("arr").is_array());
    ASSERT_EQ((int)vm.get_global("arr").as_array()->elements.size(), 3);
    ASSERT_TRUE(vm.get_global("m").is_map());
    ASSERT_TRUE(vm.get_global("s").is_string());
    ASSERT_TRUE(vm.get_global("s").as_string()->value == "helloworld");
}
