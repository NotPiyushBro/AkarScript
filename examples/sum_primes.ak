include "math_utils.ak"

let sum = 0
let count = 0
let n = 2

while (n <= 100000) {
    if (is_prime(n)) {
        sum = sum + n
        count = count + 1
    }
    n = n + 1
}

print("Primes found: " + to_string(count))
print("Sum of primes from 1 to 100000 = " + to_string(sum))
