// ============================================================
// Akar Script - Basics
// Variables, arithmetic, strings, and output
// ============================================================

// --- Variables ---
// Use 'let' to declare variables. Types are inferred.
let x = 10
let y = 3.14
let name = "Akar"
let active = true
let nothing = nil

// --- Arithmetic ---
// Standard operators: + - * / %
let sum = 10 + 20
let diff = 50 - 17
let product = 6 * 7
let quotient = 100 / 4
let remainder = 17 % 5
print("10 + 20 = " + to_string(sum))
print("17 % 5 = " + to_string(remainder))

// --- Strings ---
// Concatenation with +
let greeting = "Hello, " + name + "!"
print(greeting)

// String indexing (0-based)
print("First char: " + greeting[0])
print("Length: " + to_string(len(greeting)))

// --- Type conversion ---
// Use to_string() to convert numbers to strings
let age = 25
print("Age: " + to_string(age))

// --- Boolean logic ---
let a = true
let b = false
print("a and b = " + to_string(a and b))
print("a or b = " + to_string(a or b))
print("not b = " + to_string(not b))

// --- Comparison operators ---
// == != < > <= >=
print("10 > 5 = " + to_string(10 > 5))
print("3 == 3 = " + to_string(3 == 3))
print("nil != nil = " + to_string(nil != nil))

// --- Nil ---
// nil represents the absence of a value
let empty = nil
print("empty is nil: " + to_string(empty == nil))
