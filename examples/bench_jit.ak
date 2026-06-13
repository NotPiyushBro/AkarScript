// Benchmark: JIT effectiveness
// Each function called 50000 times (JIT triggers at call 50)

fn add_loop(n) {
    let sum = 0
    for i in 0..n {
        sum = sum + i
    }
    return sum
}

fn mul_loop(n) {
    let prod = 1
    for i in 1..n {
        prod = prod * i
        if (prod > 1000000) {
            prod = prod - 1000000
        }
    }
    return prod
}

fn compare_loop(n) {
    let count = 0
    for i in 0..n {
        if (i < n / 2) {
            count = count + 1
        }
        if (i > n / 3) {
            count = count + 2
        }
    }
    return count
}

fn nested_loop(n) {
    let sum = 0
    for i in 0..n {
        for j in 0..n {
            sum = sum + i + j
        }
    }
    return sum
}

print("=== JIT Benchmark ===")
print("Each function called 50000 times (JIT triggers at call 50)")

for r in 0..50000 {
    add_loop(100)
}
print("add_loop(100) x 50000: done")

for r in 0..50000 {
    mul_loop(100)
}
print("mul_loop(100) x 50000: done")

for r in 0..50000 {
    compare_loop(100)
}
print("compare_loop(100) x 50000: done")

for r in 0..10000 {
    nested_loop(10)
}
print("nested_loop(10) x 10000: done")

print("Done!")
