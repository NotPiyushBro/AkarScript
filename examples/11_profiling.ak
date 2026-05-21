// ============================================================
// Akar Script - Profiling & Tracing
// ============================================================
// Run with:  akar --profile examples/11_profiling.ak
//         or akar --trace examples/11_profiling.ak

// --- Fibonacci: profile a recursive function ---
fn fibonacci(n) {
    if (n <= 1) { return n }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

// Use --profile flag to see timing data automatically.
// Or use the API for selective profiling:
profile_start()
let result = fibonacci(15)
profile_stop()

print("fib(15) = " + to_string(result))
profile_report()

// --- Signal/Effect profiling ---
profile_reset()

signal counter = 0

effect {
    let _ = counter  // read counter to register dependency
}

profile_start()
let i = 0
while (i < 10) {
    counter = i
    i = i + 1
}
profile_stop()

print("\nAfter 10 signal writes:")
profile_report()

// --- Tracing: see every event ---
trace_start()
signal temp = 100
effect {
    let _ = temp
}
temp = 200
temp = 300
trace_stop()

trace_dump()

print("Profiling demo complete!")
