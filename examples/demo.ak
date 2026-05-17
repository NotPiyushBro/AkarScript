// Akar Script Demo
// A custom game scripting language with register-based VM

// ============================================================
// Variables & Arithmetic
// ============================================================
let x = 10
let y = 20
let sum = x + y
print("Sum: " + to_string(sum))

// ============================================================
// Functions
// ============================================================
fn factorial(n) {
    if (n <= 1) { return 1 }
    return n * factorial(n - 1)
}
print("Factorial of 10: " + to_string(factorial(10)))

fn fibonacci(n) {
    let a = 0
    let b = 1
    let i = 0
    while (i < n) {
        let temp = b
        b = a + b
        a = temp
        i = i + 1
    }
    return a
}
print("Fibonacci(10): " + to_string(fibonacci(10)))

// ============================================================
// Tail Call Optimization (TCO)
// ============================================================
fn sum_tail(n, acc) {
    if (n <= 0) { return acc }
    return sum_tail(n - 1, acc + n)
}
// This would overflow the stack without TCO!
print("Sum of 1..100000 = " + to_string(sum_tail(100000, 0)))

fn fact_tail(n, acc) {
    if (n <= 1) { return acc }
    return fact_tail(n - 1, acc * n)
}
print("20! = " + to_string(fact_tail(20, 1)))

// ============================================================
// Closures (mutable captured variables)
// ============================================================
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

// ============================================================
// Arrays
// ============================================================
let arr = [10, 20, 30, 40, 50]
print("Array length: " + to_string(len(arr)))
print("First element: " + to_string(arr[0]))
push(arr, 60)
print("After push, length: " + to_string(len(arr)))

// ============================================================
// Strings
// ============================================================
let greeting = "Hello, Akar Script!"
print(greeting)
print("String length: " + to_string(len(greeting)))
print("First char: " + greeting[0])

// ============================================================
// For-in loops with ranges
// ============================================================
fn fizzbuzz(n) {
    for i in 1..n {
        if (i % 15 == 0) {
            print("FizzBuzz")
        } else if (i % 3 == 0) {
            print("Fizz")
        } else if (i % 5 == 0) {
            print("Buzz")
        } else {
            print(to_string(i))
        }
    }
}
print("--- FizzBuzz 1..15 ---")
fizzbuzz(15)

// ============================================================
// Classes
// ============================================================
class Vec2 {
    init(x, y) {
        this.x = x
        this.y = y
    }
    length() {
        return sqrt(this.x * this.x + this.y * this.y)
    }
    add(other) {
        return Vec2(this.x + other.x, this.y + other.y)
    }
    to_str() {
        return "Vec2(" + to_string(this.x) + ", " + to_string(this.y) + ")"
    }
}

let a = Vec2(3, 4)
let b = Vec2(1, 2)
let c = a.add(b)
print("a = " + a.to_str())
print("b = " + b.to_str())
print("a + b = " + c.to_str())
print("|a| = " + to_string(a.length()))

// ============================================================
// Higher-order functions
// ============================================================
fn my_map(arr, mapper) {
    let result = []
    for item in arr {
        push(result, mapper(item))
    }
    return result
}

fn my_filter(arr, predicate) {
    let result = []
    for item in arr {
        if (predicate(item)) {
            push(result, item)
        }
    }
    return result
}

let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
let doubled = my_map(numbers, fn(x) { x * 2 })
print("Doubled: " + to_string(doubled))

let evens = my_filter(numbers, fn(x) { x % 2 == 0 })
print("Evens: " + to_string(evens))

// ============================================================
// Lambdas as callbacks
// ============================================================
fn execute(callback) {
    return callback(42)
}
let result = execute(fn(x) { x * 2 })
print("Execute result: " + to_string(result))

// ============================================================
// Math functions
// ============================================================
print("sqrt(144) = " + to_string(sqrt(144)))
print("floor(3.7) = " + to_string(floor(3.7)))
print("ceil(3.2) = " + to_string(ceil(3.2)))
print("abs(-42) = " + to_string(abs(-42)))

// ============================================================
// Fibers (coroutines)
// ============================================================
fn producer(val) {
    print("Producer received: " + to_string(val))
    return val * 2
}

let fiber = fiber_create(producer)
let fiber_result = fiber_resume(fiber, 21)
print("Fiber result: " + to_string(fiber_result))
print("Fiber status: " + fiber_status(fiber))

// ============================================================
// While loops
// ============================================================
fn count_down(n) {
    while (n > 0) {
        print("T-minus " + to_string(n))
        n = n - 1
    }
    print("Liftoff!")
}
count_down(5)

print("")
print("=== Akar Script Demo Complete ===")
