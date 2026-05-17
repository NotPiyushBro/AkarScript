// Akar Script Performance Benchmark
// Tests: arithmetic, function calls, loops, arrays, maps, strings

// --- Test 1: Tight arithmetic loop ---
fn bench_arithmetic(n) {
    let sum = 0
    let i = 0
    while (i < n) {
        sum = sum + i * 2 - i / 3 + i % 7
        i = i + 1
    }
    return sum
}

// --- Test 2: Function call overhead ---
fn add_one(x) { x + 1 }
fn bench_calls(n) {
    let x = 0
    let i = 0
    while (i < n) {
        x = add_one(x)
        i = i + 1
    }
    return x
}

// --- Test 3: Array access ---
fn bench_array(n) {
    let arr = []
    let i = 0
    while (i < n) {
        push(arr, i)
        i = i + 1
    }
    let sum = 0
    i = 0
    while (i < n) {
        sum = sum + arr[i]
        i = i + 1
    }
    return sum
}

// --- Test 4: String concatenation ---
fn bench_strings(n) {
    let s = ""
    let i = 0
    while (i < n) {
        s = s + "a"
        i = i + 1
    }
    return len(s)
}

// --- Test 5: Map access ---
fn bench_map(n) {
    let m = {}
    let i = 0
    while (i < n) {
        let key = to_string(i)
        m[key] = i
        i = i + 1
    }
    let sum = 0
    i = 0
    while (i < n) {
        sum = sum + m[to_string(i)]
        i = i + 1
    }
    return sum
}

// --- Test 6: Nested loops ---
fn bench_nested(n) {
    let sum = 0
    let i = 0
    while (i < n) {
        let j = 0
        while (j < 100) {
            sum = sum + j
            j = j + 1
        }
        i = i + 1
    }
    return sum
}

// --- Test 7: Class method calls ---
class Counter {
    init(start) {
        this.value = start
    }
    increment() {
        this.value = this.value + 1
    }
    get() {
        return this.value
    }
}
fn bench_class(n) {
    let c = Counter(0)
    let i = 0
    while (i < n) {
        c.increment()
        i = i + 1
    }
    return c.get()
}

// --- Run benchmarks ---
print("=== Akar Script Benchmark ===")
print("")

let n = 100000

print("Arithmetic (" + to_string(n) + " iters): " + to_string(bench_arithmetic(n)))
print("Calls (" + to_string(n) + " iters): " + to_string(bench_calls(n)))
print("Array (" + to_string(n) + " iters): " + to_string(bench_array(n)))
print("Strings (" + to_string(10000) + " iters): " + to_string(bench_strings(10000)))
print("Map (" + to_string(n) + " iters): " + to_string(bench_map(n)))
print("Nested (" + to_string(1000) + " x 100): " + to_string(bench_nested(1000)))
print("Class (" + to_string(n) + " iters): " + to_string(bench_class(n)))

print("")
print("Benchmark complete.")
