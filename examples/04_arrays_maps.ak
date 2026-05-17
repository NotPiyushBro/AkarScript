// ============================================================
// Akar Script - Arrays, Maps & Ranges
// Collection types and built-in operations
// ============================================================

// --- Arrays ---
// Create with [] syntax, comma-separated values
let nums = [10, 20, 30, 40, 50]

// Indexing (0-based)
print("First: " + to_string(nums[0]))
print("Last: " + to_string(nums[4]))

// Length
print("Length: " + to_string(len(nums)))

// Modify elements
nums[0] = 99
print("After set: " + to_string(nums[0]))

// Push appends to the end
push(nums, 60)
print("After push, length: " + to_string(len(nums)))

// Empty array
let empty = []
print("Empty length: " + to_string(len(empty)))

// --- Iterating arrays ---
print("\n--- Iterate ---")
let fruits = ["apple", "banana", "cherry"]
for fruit in fruits {
    print("  " + fruit)
}

// --- Ranges ---
// 1..5 produces 1, 2, 3, 4, 5
print("\n--- Range sum ---")
let sum = 0
for i in 1..100 {
    sum = sum + i
}
print("Sum 1..100 = " + to_string(sum))

// --- Nested arrays ---
let matrix = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
]
print("\n--- Nested array ---")
print("matrix[1][2] = " + to_string(matrix[1][2]))

// --- Array slicing with loops ---
print("\n--- Manual slice ---")
let source = [10, 20, 30, 40, 50]
let sliced = []
for i in 1..3 {
    push(sliced, source[i])
}
print("sliced[1..3] = " + to_string(sliced))

// --- Building arrays ---
print("\n--- Build with push ---")
let squares = []
for i in 1..10 {
    push(squares, i * i)
}
print("Squares: " + to_string(squares))

// --- String arrays ---
let words = ["the", "quick", "brown", "fox"]
let sentence = ""
for w in words {
    if (sentence != "") { sentence = sentence + " " }
    sentence = sentence + w
}
print("\n--- Join ---")
print(sentence)

// --- Array search ---
print("\n--- Contains ---")
let haystack = [3, 7, 11, 15, 19, 23]
let needle = 15
let found = false
for item in haystack {
    if (item == needle) {
        found = true
        break
    }
}
print("Found " + to_string(needle) + ": " + to_string(found))

// ============================================================
// Maps
// ============================================================

// --- Map literal ---
let user = {
    "name": "Akar",
    "version": 2.0,
    "active": true
}

print("\n--- Map basics ---")
print("name: " + user["name"])
print("version: " + to_string(user["version"]))

// Modify and add entries
user["version"] = 3.0
user["lang"] = "Akar Script"
print("updated version: " + to_string(user["version"]))
print("lang: " + user["lang"])

// --- Empty map ---
let config = {}
config["debug"] = true
config["width"] = 1280
print("\n--- Empty map ---")
print("debug: " + to_string(config["debug"]))
print("width: " + to_string(config["width"]))

// --- Nested maps ---
let player = {
    "name": "Hero",
    "pos": {"x": 100, "y": 200},
    "inventory": ["sword", "shield", "potion"]
}
print("\n--- Nested map ---")
print("player: " + player["name"])
print("position: " + to_string(player["pos"]["x"]) + ", " + to_string(player["pos"]["y"]))
print("first item: " + player["inventory"][0])
