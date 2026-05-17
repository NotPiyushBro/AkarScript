// ============================================================
// Akar Script - Comprehensive Fiber Test Suite
// Each section is wrapped in a function to stay under register limits.
// ============================================================

let pass_count = 0
let fail_count = 0

fn assert_eq(actual, expected, label) {
    if (actual == expected) {
        pass_count = pass_count + 1
    } else {
        fail_count = fail_count + 1
        print("  FAIL: " + label + " - expected " + to_string(expected) + " got " + to_string(actual))
    }
}

fn assert_true(val, label) {
    if (val) {
        pass_count = pass_count + 1
    } else {
        fail_count = fail_count + 1
        print("  FAIL: " + label + " - expected true")
    }
}

fn assert_status(fiber, expected, label) {
    let s = fiber_status(fiber)
    if (s == expected) {
        pass_count = pass_count + 1
    } else {
        fail_count = fail_count + 1
        print("  FAIL: " + label + " status - expected " + expected + " got " + s)
    }
}

// ============================================================
// Helper: collect all values from a fiber into an array
// ============================================================
fn collect_all(f) {
    let result = []
    while (true) {
        let v = fiber_resume(f)
        if (fiber_status(f) == "done") { break }
        push(result, v)
    }
    return result
}

// ============================================================
// Helper: range generator
// ============================================================
fn range_gen(start, end) {
    let i = start
    while (i <= end) {
        fiber_yield(i)
        i = i + 1
    }
}

// ============================================================
// Helper: natural number generator (infinite)
// ============================================================
fn natural_numbers() {
    let n = 1
    while (true) {
        fiber_yield(n)
        n = n + 1
    }
}

// ============================================================
// Helper: fiber map
// ============================================================
fn fiber_map(src_fiber, transform) {
    return fiber_create(fn() {
        while (true) {
            let v = fiber_resume(src_fiber)
            if (fiber_status(src_fiber) == "done") { break }
            fiber_yield(transform(v))
        }
    })
}

// ============================================================
// Helper: fiber filter
// ============================================================
fn fiber_filter(src_fiber, predicate) {
    return fiber_create(fn() {
        while (true) {
            let v = fiber_resume(src_fiber)
            if (fiber_status(src_fiber) == "done") { break }
            if (predicate(v)) {
                fiber_yield(v)
            }
        }
    })
}

// ============================================================
// Helper: fiber take
// ============================================================
fn fiber_take(src_fiber, count) {
    return fiber_create(fn() {
        let i = 0
        while (i < count) {
            let v = fiber_resume(src_fiber)
            if (fiber_status(src_fiber) == "done") { break }
            fiber_yield(v)
            i = i + 1
        }
    })
}

// ============================================================
// Helper: fiber reduce
// ============================================================
fn fiber_reduce(src_fiber, initial, reducer) {
    let acc = initial
    while (true) {
        let v = fiber_resume(src_fiber)
        if (fiber_status(src_fiber) == "done") { break }
        acc = reducer(acc, v)
    }
    return acc
}

print("=== Fiber Test Suite ===")

// ============================================================
// SECTION 1: Basic fiber create and resume
// ============================================================
fn test_basic_create_resume() {
    print("--- Section 1: Basic fiber create/resume ---")

    fn return_value() { return 42 }
    let f1 = fiber_create(return_value)
    assert_status(f1, "created", "initial status")
    let r1 = fiber_resume(f1)
    assert_eq(r1, 42, "return value")
    assert_status(f1, "done", "done after return")

    fn return_string() { return "hello" }
    let f2 = fiber_create(return_string)
    assert_eq(fiber_resume(f2), "hello", "string return")

    fn return_nil() { let x = 1 + 2 }
    let f3 = fiber_create(return_nil)
    assert_eq(fiber_resume(f3), nil, "implicit nil return")

    fn echo(val) { return val }
    let f4 = fiber_create(echo)
    assert_eq(fiber_resume(f4, 99), 99, "resume with arg")
    assert_eq(fiber_resume(f4, "abc"), nil, "resume done returns nil")
}
test_basic_create_resume()

// ============================================================
// SECTION 2: Simple generators
// ============================================================
fn test_simple_generators() {
    print("--- Section 2: Simple generators ---")

    fn collect_range(s, e) {
        let f = fiber_create(range_gen, s, e)
        let result = []
        while (true) {
            let v = fiber_resume(f)
            if (fiber_status(f) == "done") { break }
            push(result, v)
        }
        return result
    }

    let vals = collect_range(1, 5)
    assert_eq(len(vals), 5, "range 1..5 count")
    assert_eq(vals[0], 1, "range[0]")
    assert_eq(vals[4], 5, "range[4]")

    fn single_gen() {
        fiber_yield(100)
        return -1
    }
    let fs = fiber_create(single_gen)
    assert_eq(fiber_resume(fs), 100, "single yield")
    assert_eq(fiber_resume(fs), -1, "single return")
    assert_status(fs, "done", "single done")

    fn empty_gen() { return "done" }
    let fe = fiber_create(empty_gen)
    assert_eq(fiber_resume(fe), "done", "empty gen")
}
test_simple_generators()

// ============================================================
// SECTION 3: Fibonacci generator
// ============================================================
fn test_fibonacci() {
    print("--- Section 3: Fibonacci generator ---")

    fn fib_gen() {
        let a = 0
        let b = 1
        while (true) {
            fiber_yield(a)
            let temp = b
            b = a + b
            a = temp
        }
    }

    fn collect_fib(count) {
        let f = fiber_create(fib_gen)
        let result = []
        for i in 1..count {
            push(result, fiber_resume(f))
        }
        return result
    }

    let fibs = collect_fib(15)
    assert_eq(fibs[0], 0, "fib[0]")
    assert_eq(fibs[1], 1, "fib[1]")
    assert_eq(fibs[2], 1, "fib[2]")
    assert_eq(fibs[3], 2, "fib[3]")
    assert_eq(fibs[4], 3, "fib[4]")
    assert_eq(fibs[5], 5, "fib[5]")
    assert_eq(fibs[6], 8, "fib[6]")
    assert_eq(fibs[7], 13, "fib[7]")
    assert_eq(fibs[8], 21, "fib[8]")
    assert_eq(fibs[9], 34, "fib[9]")
    assert_eq(fibs[10], 55, "fib[10]")
    assert_eq(fibs[11], 89, "fib[11]")
    assert_eq(fibs[12], 144, "fib[12]")
    assert_eq(fibs[13], 233, "fib[13]")
    assert_eq(fibs[14], 377, "fib[14]")
}
test_fibonacci()

// ============================================================
// SECTION 4: Bidirectional communication
// ============================================================
fn test_bidirectional() {
    print("--- Section 4: Bidirectional communication ---")

    fn accumulator() {
        let total = 0
        while (true) {
            let val = fiber_yield(total)
            if (val == nil) { break }
            total = total + val
        }
        return total
    }

    let facc = fiber_create(accumulator)
    assert_eq(fiber_resume(facc), 0, "acc initial")
    assert_eq(fiber_resume(facc, 10), 10, "acc +10")
    assert_eq(fiber_resume(facc, 20), 30, "acc +20")
    assert_eq(fiber_resume(facc, 5), 35, "acc +5")
    assert_eq(fiber_resume(facc, -15), 20, "acc -15")

    fn doubler() {
        let result = 0
        while (true) {
            let val = fiber_yield(result)
            if (val == nil) { break }
            result = val * 2
        }
        return 0
    }

    let fd = fiber_create(doubler)
    assert_eq(fiber_resume(fd), 0, "doubler init")
    assert_eq(fiber_resume(fd, 5), 10, "doubler 5")
    assert_eq(fiber_resume(fd, 21), 42, "doubler 21")
    assert_eq(fiber_resume(fd, 100), 200, "doubler 100")
}
test_bidirectional()

// ============================================================
// SECTION 5: Nested fibers
// ============================================================
fn test_nested_fibers() {
    print("--- Section 5: Nested fibers ---")

    fn inner_gen(init) {
        let i = init
        while (i <= init + 2) {
            fiber_yield(i)
            i = i + 1
        }
        return init + 100
    }

    fn outer_gen(init) {
        let inner = fiber_create(inner_gen, init)
        while (true) {
            let v = fiber_resume(inner)
            if (fiber_status(inner) == "done") {
                return v
            }
            fiber_yield(v * 10)
        }
    }

    let fnested = fiber_create(outer_gen, 1)
    assert_eq(fiber_resume(fnested), 10, "nested [0]")
    assert_eq(fiber_resume(fnested), 20, "nested [1]")
    assert_eq(fiber_resume(fnested), 30, "nested [2]")
    assert_eq(fiber_resume(fnested), 101, "nested final")
    assert_status(fnested, "done", "nested done")

    // 3-level deep nesting
    fn level3() {
        fiber_yield("L3-a")
        fiber_yield("L3-b")
        return "L3-done"
    }

    fn level2() {
        let f3 = fiber_create(level3)
        while (true) {
            let v = fiber_resume(f3)
            if (fiber_status(f3) == "done") { return v }
            fiber_yield(v)
        }
    }

    fn level1() {
        let f2 = fiber_create(level2)
        while (true) {
            let v = fiber_resume(f2)
            if (fiber_status(f2) == "done") { return v }
            fiber_yield(v)
        }
    }

    let fdeep = fiber_create(level1)
    assert_eq(fiber_resume(fdeep), "L3-a", "deep 0")
    assert_eq(fiber_resume(fdeep), "L3-b", "deep 1")
    assert_eq(fiber_resume(fdeep), "L3-done", "deep final")
    assert_status(fdeep, "done", "deep done")
}
test_nested_fibers()

// ============================================================
// SECTION 6: Pipelines
// ============================================================
fn test_pipelines() {
    print("--- Section 6: Pipelines ---")

    fn source_gen() {
        let i = 1
        while (i <= 5) {
            fiber_yield(i)
            i = i + 1
        }
    }

    fn transform_gen(src_fiber, multiplier) {
        while (true) {
            let v = fiber_resume(src_fiber)
            if (fiber_status(src_fiber) == "done") { break }
            fiber_yield(v * multiplier)
        }
    }

    // Single stage pipeline: source -> x3
    let psrc = fiber_create(source_gen)
    let ptransform = fiber_create(transform_gen, psrc, 3)
    let pipeline_result = collect_all(ptransform)
    assert_eq(len(pipeline_result), 5, "pipeline count")
    assert_eq(pipeline_result[0], 3, "pipeline[0]")
    assert_eq(pipeline_result[1], 6, "pipeline[1]")
    assert_eq(pipeline_result[2], 9, "pipeline[2]")
    assert_eq(pipeline_result[3], 12, "pipeline[3]")
    assert_eq(pipeline_result[4], 15, "pipeline[4]")

    // Double pipeline: source -> x2 -> x10
    let psrc2 = fiber_create(source_gen)
    let stage1 = fiber_create(transform_gen, psrc2, 2)
    let stage2 = fiber_create(transform_gen, stage1, 10)
    let double_pipe = collect_all(stage2)
    assert_eq(double_pipe[0], 20, "double pipe[0]")
    assert_eq(double_pipe[1], 40, "double pipe[1]")
    assert_eq(double_pipe[2], 60, "double pipe[2]")
    assert_eq(double_pipe[3], 80, "double pipe[3]")
    assert_eq(double_pipe[4], 100, "double pipe[4]")
}
test_pipelines()

// ============================================================
// SECTION 7: Fan-in
// ============================================================
fn test_fan_in() {
    print("--- Section 7: Fan-in ---")

    fn produce_values(start, count) {
        let i = 0
        while (i < count) {
            fiber_yield(start + i)
            i = i + 1
        }
    }

    fn fan_in_consumer(fibers) {
        let result = []
        let active = len(fibers)
        let idx = 0
        while (active > 0) {
            if (idx >= len(fibers)) { idx = 0 }
            if (fiber_status(fibers[idx]) != "done") {
                let v = fiber_resume(fibers[idx])
                if (fiber_status(fibers[idx]) != "done") {
                    push(result, v)
                } else {
                    active = active - 1
                }
            }
            idx = idx + 1
        }
        return result
    }

    let pa = fiber_create(produce_values, 100, 3)
    let pb = fiber_create(produce_values, 200, 3)
    let fan_result = fan_in_consumer([pa, pb])
    assert_eq(len(fan_result), 6, "fan-in total")
    assert_eq(fan_result[0], 100, "fan-in [0]")
    assert_eq(fan_result[1], 200, "fan-in [1]")
    assert_eq(fan_result[2], 101, "fan-in [2]")
    assert_eq(fan_result[3], 201, "fan-in [3]")
    assert_eq(fan_result[4], 102, "fan-in [4]")
    assert_eq(fan_result[5], 202, "fan-in [5]")
}
test_fan_in()

// ============================================================
// SECTION 8: State machine
// ============================================================
fn test_state_machine() {
    print("--- Section 8: State machine ---")

    fn traffic_light() {
        while (true) {
            fiber_yield("green")
            fiber_yield("yellow")
            fiber_yield("red")
        }
    }

    let light = fiber_create(traffic_light)
    let states = []
    for i in 1..9 {
        push(states, fiber_resume(light))
    }
    assert_eq(states[0], "green", "light[0]")
    assert_eq(states[1], "yellow", "light[1]")
    assert_eq(states[2], "red", "light[2]")
    assert_eq(states[3], "green", "light[3]")
    assert_eq(states[8], "red", "light[8]")

    // Vending machine
    fn vending_machine() {
        let state = "idle"
        let balance = 0
        let price = 25
        while (true) {
            if (state == "idle") {
                let action = fiber_yield("idle: waiting")
                if (action == "insert") { state = "inserted" }
            } else if (state == "inserted") {
                let amount = fiber_yield("inserted: balance=" + to_string(balance))
                if (amount == nil) { break }
                balance = balance + amount
                if (balance >= price) { state = "dispense" }
            } else if (state == "dispense") {
                let change = balance - price
                balance = 0
                state = "idle"
                fiber_yield("dispensed! change=" + to_string(change))
            }
        }
        return "shutdown"
    }

    let vm = fiber_create(vending_machine)
    assert_eq(fiber_resume(vm), "idle: waiting", "vm idle")
    assert_eq(fiber_resume(vm, "insert"), "inserted: balance=0", "vm insert")
    assert_eq(fiber_resume(vm, 10), "inserted: balance=10", "vm 10")
    assert_eq(fiber_resume(vm, 10), "inserted: balance=20", "vm 20")
    assert_eq(fiber_resume(vm, 10), "dispensed! change=5", "vm dispense")
    assert_eq(fiber_resume(vm), "idle: waiting", "vm back to idle")
}
test_state_machine()

// ============================================================
// SECTION 9: Cooperative scheduler
// ============================================================
fn test_cooperative_scheduler() {
    print("--- Section 9: Cooperative scheduler ---")

    fn task(name, steps) {
        let i = 0
        while (i < steps) {
            fiber_yield(name + " step " + to_string(i))
            i = i + 1
        }
        return name + " done"
    }

    fn round_robin(tasks) {
        let log = []
        let active = len(tasks)
        while (active > 0) {
            for i in 0..len(tasks) - 1 {
                if (fiber_status(tasks[i]) != "done") {
                    let v = fiber_resume(tasks[i])
                    push(log, v)
                    if (fiber_status(tasks[i]) == "done") {
                        active = active - 1
                    }
                }
            }
        }
        return log
    }

    let ta = fiber_create(task, "T1", 3)
    let tb = fiber_create(task, "T2", 2)
    let tc = fiber_create(task, "T3", 1)
    let sched_log = round_robin([ta, tb, tc])
    assert_true(len(sched_log) >= 6, "scheduler log entries")
    assert_status(ta, "done", "task_a done")
    assert_status(tb, "done", "task_b done")
    assert_status(tc, "done", "task_c done")
}
test_cooperative_scheduler()

// ============================================================
// SECTION 10: Fibers with closures
// ============================================================
fn test_closures() {
    print("--- Section 10: Fibers with closures ---")

    fn make_fiber_with_state(initial) {
        let state = initial
        return fiber_create(fn() {
            while (true) {
                let cmd = fiber_yield(state)
                if (cmd == nil) { break }
                if (cmd == "reset") {
                    state = initial
                } else {
                    state = state + cmd
                }
            }
            return state
        })
    }

    let fws = make_fiber_with_state(10)
    assert_eq(fiber_resume(fws), 10, "state initial")
    assert_eq(fiber_resume(fws, 5), 15, "state +5")
    assert_eq(fiber_resume(fws, 3), 18, "state +3")
    assert_eq(fiber_resume(fws, "reset"), 10, "state reset")
    assert_eq(fiber_resume(fws, 100), 110, "state +100")

    // Filter fiber capturing predicate
    fn make_filter(src, pred) {
        return fiber_create(fn() {
            while (true) {
                let v = fiber_resume(src)
                if (fiber_status(src) == "done") { break }
                if (pred(v)) { fiber_yield(v) }
            }
        })
    }

    fn is_even(n) { n % 2 == 0 }
    let num_src = fiber_create(range_gen, -3, 8)
    let evens = make_filter(num_src, is_even)
    let even_vals = collect_all(evens)
    assert_eq(len(even_vals), 6, "filtered evens count")
    assert_eq(even_vals[0], -2, "even[0]")
    assert_eq(even_vals[1], 0, "even[1]")
    assert_eq(even_vals[2], 2, "even[2]")
    assert_eq(even_vals[5], 8, "even[5]")
}
test_closures()

// ============================================================
// SECTION 11: Functional patterns over fibers
// ============================================================
fn test_functional_patterns() {
    print("--- Section 11: Functional patterns ---")

    // range(1,100) -> filter(odd) -> map(x*x) -> take(5) -> reduce(+)
    let r100 = fiber_create(range_gen, 1, 100)
    let odds = fiber_filter(r100, fn(x) { x % 2 != 0 })
    let squared = fiber_map(odds, fn(x) { x * x })
    let first5 = fiber_take(squared, 5)
    let sum_sq = fiber_reduce(first5, 0, fn(a, b) { a + b })
    assert_eq(sum_sq, 165, "sum first 5 odd squares")

    // range(1,10) -> map(x*2) -> filter(>10) -> collect
    let r10 = fiber_create(range_gen, 1, 10)
    let doubled = fiber_map(r10, fn(x) { x * 2 })
    let gt10 = fiber_filter(doubled, fn(x) { x > 10 })
    let big_vals = collect_all(gt10)
    assert_eq(len(big_vals), 5, "gt10 count")
    assert_eq(big_vals[0], 12, "gt10[0]")
    assert_eq(big_vals[4], 20, "gt10[4]")
}
test_functional_patterns()

// ============================================================
// SECTION 12: Stress test
// ============================================================
fn test_stress() {
    print("--- Section 12: Stress test ---")

    fn counter_gen(limit) {
        let i = 0
        while (i < limit) {
            fiber_yield(i)
            i = i + 1
        }
        return limit
    }

    // 100 yields
    let s1 = fiber_create(counter_gen, 100)
    let stress_sum = 0
    for i in 1..100 {
        stress_sum = stress_sum + fiber_resume(s1)
    }
    assert_eq(stress_sum, 4950, "stress sum 0..99")
    assert_eq(fiber_resume(s1), 100, "stress final")
    assert_status(s1, "done", "stress done")

    // 500 yields
    let s2 = fiber_create(counter_gen, 500)
    let count2 = 0
    while (true) {
        let v = fiber_resume(s2)
        if (fiber_status(s2) == "done") { break }
        count2 = count2 + 1
    }
    assert_eq(count2, 500, "stress 500 yields")

    // Deep nested stress
    fn deep_inner(n) {
        let i = 0
        while (i < n) {
            fiber_yield(i * 3)
            i = i + 1
        }
        return n * 1000
    }

    fn deep_mid(n) {
        let inner = fiber_create(deep_inner, n)
        let i = 0
        while (i < n) {
            let v = fiber_resume(inner)
            if (fiber_status(inner) == "done") { return v }
            fiber_yield(v + 100)
            i = i + 1
        }
        let fv = fiber_resume(inner)
        return fv + 500
    }

    fn deep_outer(n) {
        let mid = fiber_create(deep_mid, n)
        let i = 0
        while (i < n) {
            let v = fiber_resume(mid)
            if (fiber_status(mid) == "done") { return v }
            fiber_yield(v + 1000)
            i = i + 1
        }
        let fv = fiber_resume(mid)
        return fv + 5000
    }

    let deep = fiber_create(deep_outer, 10)
    let deep_vals = []
    while (true) {
        let v = fiber_resume(deep)
        if (fiber_status(deep) == "done") {
            break
        }
        push(deep_vals, v)
    }
    assert_eq(len(deep_vals), 10, "deep stress 10")
    assert_eq(deep_vals[0], 1100, "deep[0]")
    assert_eq(deep_vals[1], 1103, "deep[1]")
}
test_stress()

// ============================================================
// SECTION 13: Generator composition
// ============================================================
fn test_composition() {
    print("--- Section 13: Generator composition ---")

    fn enumerate(src) {
        return fiber_create(fn() {
            let idx = 0
            while (true) {
                let v = fiber_resume(src)
                if (fiber_status(src) == "done") { break }
                fiber_yield([idx, v])
                idx = idx + 1
            }
        })
    }

    fn zip_fibers(a, b) {
        return fiber_create(fn() {
            while (true) {
                let va = fiber_resume(a)
                let vb = fiber_resume(b)
                if (fiber_status(a) == "done") { break }
                if (fiber_status(b) == "done") { break }
                fiber_yield([va, vb])
            }
        })
    }

    fn chain_fibers(a, b) {
        return fiber_create(fn() {
            while (true) {
                let v = fiber_resume(a)
                if (fiber_status(a) == "done") { break }
                fiber_yield(v)
            }
            while (true) {
                let v = fiber_resume(b)
                if (fiber_status(b) == "done") { break }
                fiber_yield(v)
            }
        })
    }

    // Test enumerate
    let esrc = fiber_create(range_gen, 10, 13)
    let efib = enumerate(esrc)
    let e0 = fiber_resume(efib)
    assert_eq(e0[0], 0, "enum idx 0")
    assert_eq(e0[1], 10, "enum val 0")
    let e1 = fiber_resume(efib)
    assert_eq(e1[0], 1, "enum idx 1")
    assert_eq(e1[1], 11, "enum val 1")

    // Test zip
    let za = fiber_create(range_gen, 1, 3)
    let zb = fiber_create(range_gen, 10, 12)
    let zfib = zip_fibers(za, zb)
    let z0 = fiber_resume(zfib)
    assert_eq(z0[0], 1, "zip a[0]")
    assert_eq(z0[1], 10, "zip b[0]")
    let z1 = fiber_resume(zfib)
    assert_eq(z1[0], 2, "zip a[1]")
    assert_eq(z1[1], 11, "zip b[1]")

    // Test chain
    let ca = fiber_create(range_gen, 1, 3)
    let cb = fiber_create(range_gen, 10, 12)
    let cfib = chain_fibers(ca, cb)
    let chain_vals = collect_all(cfib)
    assert_eq(len(chain_vals), 6, "chain count")
    assert_eq(chain_vals[0], 1, "chain[0]")
    assert_eq(chain_vals[3], 10, "chain[3]")
    assert_eq(chain_vals[5], 12, "chain[5]")
}
test_composition()

// ============================================================
// SECTION 14: Binary tree iterator
// ============================================================
fn test_tree_iterator() {
    print("--- Section 14: Binary tree iterator ---")

    fn tree_fiber(tree) {
        return fiber_create(fn() {
            // Iterative in-order traversal using explicit stack
            let stack = []
            let top = 0
            let current = tree
            while (true) {
                if (current == nil) {
                    if (top == 0) { break }
                }
                while (current != nil) {
                    stack[top] = current
                    top = top + 1
                    current = current["left"]
                }
                top = top - 1
                current = stack[top]
                fiber_yield(current["value"])
                current = current["right"]
            }
        })
    }

    let bst = {
        "value": 5,
        "left": {
            "value": 3,
            "left": {"value": 1, "left": nil, "right": nil},
            "right": {"value": 4, "left": nil, "right": nil}
        },
        "right": {
            "value": 8,
            "left": {"value": 6, "left": nil, "right": nil},
            "right": {"value": 10, "left": nil, "right": nil}
        }
    }

    let tree_iter = tree_fiber(bst)
    let tree_vals = collect_all(tree_iter)
    assert_eq(len(tree_vals), 7, "BST nodes")
    assert_eq(tree_vals[0], 1, "BST[0]")
    assert_eq(tree_vals[1], 3, "BST[1]")
    assert_eq(tree_vals[2], 4, "BST[2]")
    assert_eq(tree_vals[3], 5, "BST[3]")
    assert_eq(tree_vals[4], 6, "BST[4]")
    assert_eq(tree_vals[5], 8, "BST[5]")
    assert_eq(tree_vals[6], 10, "BST[6]")
}
test_tree_iterator()

// ============================================================
// SECTION 15: Multiple independent instances
// ============================================================
fn test_multiple_instances() {
    print("--- Section 15: Multiple instances ---")

    fn shared_gen(start) {
        let i = start
        while (i <= start + 2) {
            fiber_yield(i)
            i = i + 1
        }
    }

    let ia = fiber_create(shared_gen, 10)
    let ib = fiber_create(shared_gen, 20)
    let ic = fiber_create(shared_gen, 30)

    assert_eq(fiber_resume(ia), 10, "ia[0]")
    assert_eq(fiber_resume(ib), 20, "ib[0]")
    assert_eq(fiber_resume(ic), 30, "ic[0]")
    assert_eq(fiber_resume(ia), 11, "ia[1]")
    assert_eq(fiber_resume(ib), 21, "ib[1]")
    assert_eq(fiber_resume(ic), 31, "ic[1]")
    assert_eq(fiber_resume(ia), 12, "ia[2]")
    assert_eq(fiber_resume(ib), 22, "ib[2]")
    assert_eq(fiber_resume(ic), 32, "ic[2]")
    // One more resume to trigger the return (function ends after 3 yields)
    assert_eq(fiber_resume(ia), nil, "ia return")
    assert_eq(fiber_resume(ib), nil, "ib return")
    assert_eq(fiber_resume(ic), nil, "ic return")
    assert_status(ia, "done", "ia done")
    assert_status(ib, "done", "ib done")
    assert_status(ic, "done", "ic done")
}
test_multiple_instances()

// ============================================================
// SECTION 16: Edge cases
// ============================================================
fn test_edge_cases() {
    print("--- Section 16: Edge cases ---")

    fn yield_once() {
        fiber_yield(42)
        return 99
    }
    let fyo = fiber_create(yield_once)
    assert_eq(fiber_resume(fyo), 42, "yield once")
    assert_eq(fiber_resume(fyo), 99, "return once")
    assert_status(fyo, "done", "yield once done")
    assert_eq(fiber_resume(fyo), nil, "resume done nil")
    assert_eq(fiber_resume(fyo), nil, "resume done again")

    fn many_locals() {
        let a = 1
        let b = 2
        let c = 3
        let d = 4
        let e = 5
        let f = 6
        let g = 7
        let h = 8
        let ii = 9
        let j = 10
        fiber_yield(a + b + c + d + e + f + g + h + ii + j)
        return a * b * c * d * e
    }
    let fml = fiber_create(many_locals)
    assert_eq(fiber_resume(fml), 55, "locals sum")
    assert_eq(fiber_resume(fml), 120, "locals product")

    fn mixed_types() {
        fiber_yield("hello")
        fiber_yield(42)
        fiber_yield(true)
        fiber_yield(nil)
        fiber_yield(3.14)
        return "done"
    }
    let fmt = fiber_create(mixed_types)
    assert_eq(fiber_resume(fmt), "hello", "mixed str")
    assert_eq(fiber_resume(fmt), 42, "mixed num")
    assert_eq(fiber_resume(fmt), true, "mixed bool")
    assert_eq(fiber_resume(fmt), nil, "mixed nil")
    assert_eq(fiber_resume(fmt), 3.14, "mixed float")
    assert_eq(fiber_resume(fmt), "done", "mixed return")
}
test_edge_cases()

// ============================================================
// SECTION 17: Prime sieve via fibers
// ============================================================
fn test_prime_sieve() {
    print("--- Section 17: Prime sieve ---")

    fn primes_sieve() {
        let naturals = fiber_create(natural_numbers)
        let primes = []
        fiber_resume(naturals) // skip 1
        while (true) {
            let candidate = fiber_resume(naturals)
            if (fiber_status(naturals) == "done") { break }
            let is_prime = true
            if (len(primes) > 0) {
                for i in 0..len(primes) - 1 {
                    if (candidate % primes[i] == 0) {
                        is_prime = false
                        break
                    }
                }
            }
            if (is_prime) {
                push(primes, candidate)
                fiber_yield(candidate)
            }
        }
    }

    let pfib = fiber_create(primes_sieve)
    let first_primes = []
    for i in 1..15 {
        push(first_primes, fiber_resume(pfib))
    }
    assert_eq(first_primes[0], 2, "prime[0]")
    assert_eq(first_primes[1], 3, "prime[1]")
    assert_eq(first_primes[2], 5, "prime[2]")
    assert_eq(first_primes[3], 7, "prime[3]")
    assert_eq(first_primes[4], 11, "prime[4]")
    assert_eq(first_primes[5], 13, "prime[5]")
    assert_eq(first_primes[6], 17, "prime[6]")
    assert_eq(first_primes[7], 19, "prime[7]")
    assert_eq(first_primes[8], 23, "prime[8]")
    assert_eq(first_primes[9], 29, "prime[9]")
    assert_eq(first_primes[10], 31, "prime[10]")
    assert_eq(first_primes[11], 37, "prime[11]")
    assert_eq(first_primes[12], 41, "prime[12]")
    assert_eq(first_primes[13], 43, "prime[13]")
    assert_eq(first_primes[14], 47, "prime[14]")
}
test_prime_sieve()

// ============================================================
// SECTION 18: Bidirectional protocol (request handler)
// ============================================================
fn test_request_handler() {
    print("--- Section 18: Request handler ---")

    fn request_handler() {
        let db = {}
        db["user:1"] = "Alice"
        db["user:2"] = "Bob"
        db["user:3"] = "Charlie"

        let response = nil
        while (true) {
            let request = fiber_yield(response)
            if (request == nil) { break }
            let action = request[0]
            let key = request[1]
            if (action == "get") {
                response = db[key]
            } else if (action == "set") {
                db[key] = request[2]
                response = true
            } else if (action == "del") {
                db[key] = nil
                response = true
            } else {
                response = nil
            }
        }
        return "handler closed"
    }

    let handler = fiber_create(request_handler)
    assert_eq(fiber_resume(handler), nil, "initial")

    assert_eq(fiber_resume(handler, ["get", "user:1"]), "Alice", "get user:1")
    assert_eq(fiber_resume(handler, ["get", "user:2"]), "Bob", "get user:2")
    assert_eq(fiber_resume(handler, ["set", "user:4", "Dave"]), true, "set user:4")
    assert_eq(fiber_resume(handler, ["get", "user:4"]), "Dave", "get user:4")
    assert_eq(fiber_resume(handler, ["del", "user:1"]), true, "del user:1")
    assert_eq(fiber_resume(handler, ["get", "user:1"]), nil, "get deleted")
}
test_request_handler()

// ============================================================
// SECTION 19: Buffered producer-consumer
// ============================================================
fn test_buffered() {
    print("--- Section 19: Buffered producer-consumer ---")

    fn buffered_consumer(producer_fiber, batch_size) {
        let batches = []
        let current = []
        while (true) {
            let v = fiber_resume(producer_fiber)
            if (fiber_status(producer_fiber) == "done") { break }
            push(current, v)
            if (len(current) >= batch_size) {
                push(batches, current)
                current = []
            }
        }
        if (len(current) > 0) { push(batches, current) }
        return batches
    }

    let items = ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j"]
    let prod = fiber_create(fn() {
        let i = 0
        while (i < len(items)) {
            fiber_yield(items[i])
            i = i + 1
        }
    })
    let batches = buffered_consumer(prod, 3)
    assert_eq(len(batches), 4, "batch count")
    assert_eq(batches[0][0], "a", "b[0][0]")
    assert_eq(batches[0][2], "c", "b[0][2]")
    assert_eq(batches[1][0], "d", "b[1][0]")
    assert_eq(batches[3][0], "j", "b[3][0]")
}
test_buffered()

// ============================================================
// SECTION 20: Many fibers stress
// ============================================================
fn test_many_fibers() {
    print("--- Section 20: Many fibers stress ---")

    fn simple_gen(n) {
        let i = 0
        while (i < n) {
            fiber_yield(i)
            i = i + 1
        }
        return n
    }

    // Create and resume 20 fibers
    let fibers = []
    for i in 1..20 {
        push(fibers, fiber_create(simple_gen, i))
    }

    // First round: all yield 0
    for i in 0..len(fibers) - 1 {
        let v = fiber_resume(fibers[i])
        assert_eq(v, 0, "many[" + to_string(i) + "] first")
    }

    // Second round
    for i in 1..len(fibers) - 1 {
        let v = fiber_resume(fibers[i])
        assert_eq(v, 1, "many[" + to_string(i) + "] second")
    }

    // Fiber 0 (only 1 step): needs one more resume to trigger return
    fiber_resume(fibers[0])
    assert_status(fibers[0], "done", "fiber 0 done")
}
test_many_fibers()

// ============================================================
// SECTION 21: Complex chain pipeline
// ============================================================
fn test_complex_chain() {
    print("--- Section 21: Complex chain ---")

    fn squares_gen() {
        let n = 1
        while (true) {
            fiber_yield(n * n)
            n = n + 1
        }
    }

    // squares -> filter(even) -> take(5)
    let sq = fiber_create(squares_gen)
    let sq_even = fiber_filter(sq, fn(x) { x % 2 == 0 })
    let sq_taken = fiber_take(sq_even, 5)
    let sq_result = collect_all(sq_taken)
    assert_eq(len(sq_result), 5, "even squares count")
    assert_eq(sq_result[0], 4, "esq[0]")
    assert_eq(sq_result[1], 16, "esq[1]")
    assert_eq(sq_result[2], 36, "esq[2]")
    assert_eq(sq_result[3], 64, "esq[3]")
    assert_eq(sq_result[4], 100, "esq[4]")

    // naturals -> filter(mult of 3) -> map(x*10) -> take(5)
    let nats = fiber_create(natural_numbers)
    let mult3 = fiber_filter(nats, fn(x) { x % 3 == 0 })
    let times10 = fiber_map(mult3, fn(x) { x * 10 })
    let first5 = fiber_take(times10, 5)
    let m3_result = collect_all(first5)
    assert_eq(m3_result[0], 30, "m3[0]")
    assert_eq(m3_result[1], 60, "m3[1]")
    assert_eq(m3_result[2], 90, "m3[2]")
    assert_eq(m3_result[3], 120, "m3[3]")
    assert_eq(m3_result[4], 150, "m3[4]")
}
test_complex_chain()

// ============================================================
// RESULTS
// ============================================================
print("")
print("=== Test Results ===")
print("Passed: " + to_string(pass_count))
print("Failed: " + to_string(fail_count))
if (fail_count == 0) {
    print("ALL TESTS PASSED!")
} else {
    print("SOME TESTS FAILED!")
}
