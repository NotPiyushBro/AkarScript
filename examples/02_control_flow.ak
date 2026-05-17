// ============================================================
// Akar Script - Control Flow
// if/else, while, for, for-in, break, continue
// ============================================================

// --- If / Else ---
let temperature = 35
if (temperature > 30) {
    print("It's hot outside")
} else if (temperature > 20) {
    print("Nice weather")
} else {
    print("It's cold")
}

// --- While loop ---
print("\n--- Countdown ---")
let n = 5
while (n > 0) {
    print(to_string(n) + "...")
    n = n - 1
}
print("Go!")

// --- For loop (C-style) ---
print("\n--- For loop ---")
let sum = 0
for (let i = 1; i <= 10; i = i + 1) {
    sum = sum + i
}
print("Sum 1..10 = " + to_string(sum))

// --- For-in loop ---
// Iterate over a range
print("\n--- For-in with range ---")
for i in 1..5 {
    print("i = " + to_string(i))
}

// Iterate over an array
print("\n--- For-in with array ---")
let colors = ["red", "green", "blue"]
for color in colors {
    print("Color: " + color)
}

// --- Break ---
print("\n--- Break ---")
let i = 0
while (true) {
    if (i >= 5) { break }
    print("i = " + to_string(i))
    i = i + 1
}

// --- Continue ---
print("\n--- Continue (skip even) ---")
for i in 1..10 {
    if (i % 2 == 0) { continue }
    print(to_string(i))
}

// --- Nested loops ---
print("\n--- Multiplication table (1..3) ---")
for i in 1..3 {
    let line = ""
    for j in 1..3 {
        let product = i * j
        line = line + to_string(product) + " "
    }
    print(line)
}
