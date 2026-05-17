// ============================================================
// Akar Script - Include System
// Split code across files using 'include'
// ============================================================
//
// The 'include' directive imports another .ak file at compile time.
// All top-level declarations (let, fn, class) become available.
//
// Syntax:
//   include "path/to/file.ak"     // relative to current file
//   include "../other/file.ak"    // parent directory
//
// Circular includes are detected and skipped automatically.

// Import the math utilities header
include "math_utils.ak"

// Use the imported is_prime() function
print("Is 17 prime? " + to_string(is_prime(17)))
print("Is 15 prime? " + to_string(is_prime(15)))

// Use it in a loop
print("\nPrimes from 1 to 50:")
let line = ""
for i in 1..50 {
    if (is_prime(i)) {
        line = line + to_string(i) + " "
    }
}
print(line)
