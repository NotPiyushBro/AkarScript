fn is_prime(n) {
    if (n < 2) return false
    if (n == 2) return true
    if (n % 2 == 0) return false
    let i = 3
    while (i * i <= n) {
        if (n % i == 0) return false
        i = i + 2
    }
    return true
}

// Warm up to trigger JIT
let warmup = 0
while (warmup < 60) {
    is_prime(warmup)
    warmup = warmup + 1
}

// Test after JIT
print("After JIT:")
print("is_prime(2) = " + to_string(is_prime(2)) + " expect true")
print("is_prime(3) = " + to_string(is_prime(3)) + " expect true")
print("is_prime(4) = " + to_string(is_prime(4)) + " expect false")
print("is_prime(5) = " + to_string(is_prime(5)) + " expect true")
print("is_prime(7) = " + to_string(is_prime(7)) + " expect true")
print("is_prime(9) = " + to_string(is_prime(9)) + " expect false")
print("is_prime(11) = " + to_string(is_prime(11)) + " expect true")
print("is_prime(13) = " + to_string(is_prime(13)) + " expect true")
print("is_prime(15) = " + to_string(is_prime(15)) + " expect false")
print("is_prime(17) = " + to_string(is_prime(17)) + " expect true")
print("is_prime(25) = " + to_string(is_prime(25)) + " expect false")
print("is_prime(100) = " + to_string(is_prime(100)) + " expect false")
print("is_prime(997) = " + to_string(is_prime(997)) + " expect true")
print("is_prime(100000) = " + to_string(is_prime(100000)) + " expect false")
