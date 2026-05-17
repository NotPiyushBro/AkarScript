// ============================================================
// Akar Script - Functions
// Declaration, recursion, TCO, closures, lambdas
// ============================================================

// --- Basic function ---
fn greet(name) {
    return "Hello, " + name + "!"
}
print(greet("Akar"))

// --- Multiple parameters ---
fn add(a, b) {
    return a + b
}
print("3 + 4 = " + to_string(add(3, 4)))

// --- Recursion ---
fn factorial(n) {
    if (n <= 1) { return 1 }
    return n * factorial(n - 1)
}
print("5! = " + to_string(factorial(5)))

// --- Tail Call Optimization (TCO) ---
// Recursive tail calls are optimized to avoid stack overflow.
// This can process 100,000 iterations without blowing the stack.
fn sum_tail(n, acc) {
    if (n <= 0) { return acc }
    return sum_tail(n - 1, acc + n)
}
print("Sum 1..100000 = " + to_string(sum_tail(100000, 0)))

// --- Closures ---
// Functions can capture variables from their enclosing scope.
// Captured variables are mutable.
fn make_counter(start) {
    let count = start
    return fn() {
        count = count + 1
        return count
    }
}
let counter = make_counter(0)
print("Counter: " + to_string(counter()))
print("Counter: " + to_string(counter()))
print("Counter: " + to_string(counter()))

// --- Lambda / Anonymous functions ---
// Use fn(args) { body } for inline functions
let double = fn(x) { x * 2 }
print("double(21) = " + to_string(double(21)))

// Lambdas as arguments (callbacks)
fn apply(f, value) {
    return f(value)
}
print("apply(fn(x){x*x}, 9) = " + to_string(apply(fn(x) { x * x }, 9)))

// --- Higher-order functions ---
fn map(arr, f) {
    let result = []
    for item in arr {
        push(result, f(item))
    }
    return result
}

fn filter(arr, predicate) {
    let result = []
    for item in arr {
        if (predicate(item)) {
            push(result, item)
        }
    }
    return result
}

let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
let squared = map(numbers, fn(x) { x * x })
print("Squared: " + to_string(squared))

let odds = filter(numbers, fn(x) { x % 2 != 0 })
print("Odds: " + to_string(odds))

// --- Implicit return ---
// The last expression in a function body is returned automatically
fn cube(x) { x * x * x }
print("cube(3) = " + to_string(cube(3)))
