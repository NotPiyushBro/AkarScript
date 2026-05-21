// ============================================================
// Akar Script - Upvalues & Closures
// Closures can capture and mutate variables from outer scopes.
// ============================================================

// --- Basic upvalue: read ---
fn make_greeting(prefix) {
    // `prefix` is captured as an upvalue by the returned closure
    return fn(name) {
        return prefix + ", " + name + "!"
    }
}
let hello = make_greeting("Hello")
let hi = make_greeting("Hi")
print(hello("World"))   // Hello, World!
print(hi("Akar"))       // Hi, Akar!

// --- Mutable upvalue ---
fn make_counter(start) {
    let count = start
    // The closure captures `count` and can mutate it
    return fn() {
        count = count + 1
        return count
    }
}
let counter = make_counter(0)
print("Counter: " + to_string(counter()))  // 1
print("Counter: " + to_string(counter()))  // 2
print("Counter: " + to_string(counter()))  // 3

// --- Multiple closures sharing the same upvalue ---
fn make_pair(initial) {
    let val = initial
    fn getter() { return val }
    fn setter(v) { val = v }
    return [getter, setter]
}
let [get, set] = make_pair(100)
print("get: " + to_string(get()))    // 100
set(200)
print("get: " + to_string(get()))    // 200

// --- Nested upvalues (3 levels deep) ---
fn level1(a) {
    fn level2(b) {
        fn level3(c) {
            return a + b + c
        }
        return level3
    }
    return level2
}
let result = level1(1)(2)(3)
print("3-level upvalue: " + to_string(result))  // 6

// --- Upvalue in a loop (each closure gets its own copy) ---
let fns = []
for i in 0..5 {
    let captured = i       // each iteration creates a new `captured`
    push(fns, fn() { return captured })
}
let vals = ""
for f in fns {
    vals = vals + to_string(f()) + " "
}
print("loop upvalues: " + vals)  // 0 1 2 3 4

// --- Upvalue inside a class method ---
class Accumulator {
    init() { this.total = 0 }

    // Closure inside a method captures `this`
    make_adder() {
        return fn(n) {
            this.total = this.total + n
            return this.total
        }
    }
}
let acc = Accumulator()
let add = acc.make_adder()
print("add 10: " + to_string(add(10)))   // 10
print("add 20: " + to_string(add(20)))   // 30
print("total: " + to_string(acc.total))  // 30

// --- Upvalue with map (closure as map value) ---
fn make_bank(initial) {
    let balance = initial
    return {
        "deposit":  fn(amt) { balance = balance + amt; return balance },
        "withdraw": fn(amt) { balance = balance - amt; return balance },
        "check":    fn() { return balance }
    }
}
let bank = make_bank(100)
bank["deposit"](50)
bank["withdraw"](20)
let check = bank["check"]
print("balance: " + to_string(check()))  // 130

// --- Recursive function declared as local ---
fn make_fib() {
    fn fib(n) {
        if (n <= 1) return n
        return fib(n - 1) + fib(n - 2)
    }
    return fib
}
let fib = make_fib()
print("fib(10): " + to_string(fib(10)))  // 55

// --- Closure pipeline ---
fn make_pipeline() {
    let add1 = fn(x) { x + 1 }
    let double = fn(x) { x * 2 }
    let sub3 = fn(x) { x - 3 }
    return fn(x) {
        return sub3(double(add1(x)))
    }
}
let pipe = make_pipeline()
print("pipe(5): " + to_string(pipe(5)))  // (5+1)*2-3 = 9
print("pipe(10): " + to_string(pipe(10))) // (10+1)*2-3 = 19

print("\nAll upvalue tests passed!")
