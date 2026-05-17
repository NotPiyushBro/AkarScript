// ============================================================
// Akar Script - Advanced Features
// Fibers, closures, functional patterns
// ============================================================

// --- Fibers (Coroutines) ---
// Fibers allow cooperative multitasking.
// fiber_create(fn) creates a fiber from a function.
// fiber_resume(fiber, value) runs the fiber and returns its result.
// fiber_status(fiber) returns "suspended" or "done".

fn double_it(val) {
    return val * 2
}

let fiber = fiber_create(double_it)
let result = fiber_resume(fiber, 21)
print("Fiber result: " + to_string(result))
print("Fiber status: " + fiber_status(fiber))

// --- Closure-based iterator ---
fn make_range_iter(start, end) {
    let current = start
    return fn() {
        if (current > end) { return nil }
        let val = current
        current = current + 1
        return val
    }
}

print("\n--- Closure iterator ---")
let next = make_range_iter(1, 5)
while (true) {
    let val = next()
    if (val == nil) { break }
    print("  " + to_string(val))
}

// --- Accumulator pattern ---
fn make_accumulator(initial) {
    let total = initial
    return fn(amount) {
        total = total + amount
        return total
    }
}

print("\n--- Accumulator ---")
let acc = make_accumulator(0)
print("acc(10) = " + to_string(acc(10)))
print("acc(20) = " + to_string(acc(20)))
print("acc(5) = " + to_string(acc(5)))

// --- Compose: combine two functions ---
fn compose(f, g) {
    return fn(x) { f(g(x)) }
}

let add_one = fn(x) { x + 1 }
let times_two = fn(x) { x * 2 }
let add_one_then_double = compose(times_two, add_one)
print("\n--- Compose ---")
print("(5+1)*2 = " + to_string(add_one_then_double(5)))

// --- Partial application ---
fn partial_add(a) {
    return fn(b) { a + b }
}
let add_10 = partial_add(10)
print("\n--- Partial application ---")
print("add_10(5) = " + to_string(add_10(5)))
print("add_10(20) = " + to_string(add_10(20)))

// --- Curry: turn multi-arg function into chain ---
fn curry_add(a) {
    return fn(b) {
        return fn(c) { a + b + c }
    }
}

print("\n--- Curry ---")
let add_1_2 = curry_add(1)(2)
print("1+2+3 = " + to_string(add_1_2(3)))

// --- Generator: yield multiple values via closure ---
fn make_fibonacci_gen() {
    let a = 0
    let b = 1
    return fn() {
        let val = a
        let temp = b
        b = a + b
        a = temp
        return val
    }
}

print("\n--- Fibonacci generator ---")
let fib = make_fibonacci_gen()
let line = ""
for i in 1..15 {
    line = line + to_string(fib()) + " "
}
print(line)

// --- Once: function that runs only once ---
fn make_once(f) {
    let called = false
    let saved = nil
    return fn() {
        if (called) { return saved }
        called = true
        saved = f()
        return saved
    }
}

let expensive = make_once(fn() {
    print("  (computing...)")
    return 42
})

print("\n--- Once ---")
print("First call: " + to_string(expensive()))
print("Second call: " + to_string(expensive()))
print("Third call: " + to_string(expensive()))
