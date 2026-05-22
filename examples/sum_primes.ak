include "math_utils.ak"

let start = clock()

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

let elapsed = (clock() - start) * 1000

print("Primes found: " + to_string(count))
print("Sum of primes from 1 to 100000 = " + to_string(sum))
print("Time: " + to_string(elapsed) + " ms")
