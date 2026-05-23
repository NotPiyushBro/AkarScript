// ============================================================
// Akar Script - GC Stress Test
// Exercises the garbage collector with heavy allocation pressure
// ============================================================

print("=== GC Stress Test ===")
print("")

// --- Test 1: Rapid array allocation ---
// Creates and discards thousands of arrays to trigger GC
print("--- Test 1: Array churn ---")
let sum = 0
for i in 1..5000 {
    let arr = [i, i+1, i+2, i+3, i+4]
    sum = sum + arr[0] + arr[4]
    // arr goes out of scope here → becomes garbage
}
print("Array churn sum: " + to_string(sum))
if (sum == 25025000) {
    print("  PASS")
} else {
    print("  FAIL: expected 25025000 got " + to_string(sum))
}

// --- Test 2: Map allocation and discard ---
print("\n--- Test 2: Map churn ---")
let count = 0
for i in 1..3000 {
    let m = {"a": i, "b": i * 2, "c": i * 3}
    if (m["b"] == i * 2) {
        count = count + 1
    }
    // m becomes garbage each iteration
}
print("Map churn count: " + to_string(count))
if (count == 3000) {
    print("  PASS")
} else {
    print("  FAIL: expected 3000 got " + to_string(count))
}

// --- Test 3: String allocation churn ---
print("\n--- Test 3: String churn ---")
let total_len = 0
for i in 1..2000 {
    let s = "hello_" + to_string(i) + "_world"
    total_len = total_len + len(s)
    // s becomes garbage
}
print("String churn total_len: " + to_string(total_len))
// Each string is roughly 6 + digits + 6 chars
if (total_len > 20000) {
    print("  PASS (total_len=" + to_string(total_len) + ")")
} else {
    print("  FAIL: total_len too small: " + to_string(total_len))
}

// --- Test 4: Nested array allocation ---
print("\n--- Test 4: Nested arrays ---")
let nested_sum = 0
for i in 1..1000 {
    let outer = []
    for j in 1..10 {
        push(outer, [j, j * 2, j * 3])
    }
    nested_sum = nested_sum + outer[9][2]
    // outer and all inner arrays become garbage
}
print("Nested sum: " + to_string(nested_sum))
if (nested_sum == 30000) {
    print("  PASS")
} else {
    print("  FAIL: expected 30000 got " + to_string(nested_sum))
}

// --- Test 5: Closure allocation ---
print("\n--- Test 5: Closure churn ---")
let closure_sum = 0
for i in 1..2000 {
    let fn_a = fn(x) { x + i }
    let fn_b = fn(x) { fn_a(x) * 2 }
    closure_sum = closure_sum + fn_b(10)
    // fn_a, fn_b, and captured i become garbage
}
print("Closure sum: " + to_string(closure_sum))
// fn_b(10) = (10 + i) * 2. Sum = sum((10+i)*2) for i=1..2000 = 2*sum(10+i) = 2*(20000+2001000) = 4042000
if (closure_sum == 4042000) {
    print("  PASS")
} else {
    print("  FAIL: expected 4042000 got " + to_string(closure_sum))
}

// --- Test 6: Class instance churn ---
print("\n--- Test 6: Class instances ---")
class Point {
    init(x, y) {
        this.x = x
        this.y = y
    }
    dist_sq() {
        return this.x * this.x + this.y * this.y
    }
}

let max_dist = 0
for i in 1..3000 {
    let p = Point(i, i + 1)
    let d = p.dist_sq()
    if (d > max_dist) {
        max_dist = d
    }
    // p becomes garbage
}
print("Max dist_sq: " + to_string(max_dist))
let expected = 3000 * 3000 + 3001 * 3001
if (max_dist == expected) {
    print("  PASS")
} else {
    print("  FAIL: expected " + to_string(expected) + " got " + to_string(max_dist))
}

// --- Test 7: Mixed allocation types ---
print("\n--- Test 7: Mixed allocations ---")
let mixed_ok = true
for i in 1..2000 {
    let arr = [i, i + 1]
    let m = {"val": i}
    let s = "item_" + to_string(i)
    let p = Point(arr[0], m["val"])
    let f = fn() { arr[0] + m["val"] }
    
    if (p.x != i) { mixed_ok = false }
    if (f() != i + i) { mixed_ok = false }
    if (len(s) < 5) { mixed_ok = false }
}
if (mixed_ok) {
    print("  PASS")
} else {
    print("  FAIL: mixed allocation checks failed")
}

// --- Test 8: Large array with GC pressure ---
print("\n--- Test 8: Large arrays ---")
let big = []
for i in 1..500 {
    push(big, [i, i * 2, i * 3, i * 4, i * 5])
}
// Now allocate many small objects while big is alive
let temp_sum = 0
for i in 1..5000 {
    let temp = [i, i + 1, i + 2]
    temp_sum = temp_sum + temp[2]
}
// Verify big is still intact
let big_ok = true
for i in 0..499 {
    if (big[i][0] != i + 1) { big_ok = false }
    if (big[i][4] != (i + 1) * 5) { big_ok = false }
}
print("Temp sum: " + to_string(temp_sum))
print("Big array intact: " + to_string(big_ok))
if (big_ok and temp_sum == 12512500) {
    print("  PASS")
} else {
    print("  FAIL")
}

// --- Test 9: Map with object values ---
print("\n--- Test 9: Map with objects ---")
let obj_map = {}
for i in 1..1000 {
    obj_map[to_string(i)] = Point(i, i * 2)
}
// Verify a few entries
let map_ok = true
if (obj_map["1"].x != 1) { map_ok = false }
if (obj_map["500"].y != 1000) { map_ok = false }
if (obj_map["1000"].x != 1000) { map_ok = false }
if (map_ok) {
    print("  PASS")
} else {
    print("  FAIL: map object values corrupted")
}

// --- Test 10: Overwrite references (force collection of old values) ---
print("\n--- Test 10: Overwrite references ---")
let holder = []
for i in 1..100 {
    push(holder, "initial_" + to_string(i))
}
// Overwrite all entries with new values
for i in 0..99 {
    holder[i] = "replaced_" + to_string(i + 1)
}
// Original strings should be collectable
let overwrite_ok = true
if (holder[0] != "replaced_1") { overwrite_ok = false }
if (holder[99] != "replaced_100") { overwrite_ok = false }
if (overwrite_ok) {
    print("  PASS")
} else {
    print("  FAIL: overwrite check failed")
}

// --- Test 11: Self-referencing closure pattern ---
print("\n--- Test 11: Closure chains ---")
fn make_chain(n) {
    let val = n
    let inner = fn() { return val }
    return fn() {
        return inner() * 2
    }
}
let chain_sum = 0
for i in 1..2000 {
    let f = make_chain(i)
    chain_sum = chain_sum + f()
}
print("Chain sum: " + to_string(chain_sum))
if (chain_sum == 4002000) {
    print("  PASS")
} else {
    print("  FAIL: expected 4002000 got " + to_string(chain_sum))
}

// --- Test 12: Array of closures ---
print("\n--- Test 12: Array of closures ---")
let fns = []
for i in 1..500 {
    let captured = i
    push(fns, fn(x) { x * captured })
}
let fn_sum = 0
for i in 0..499 {
    fn_sum = fn_sum + fns[i](3)
}
print("Fn array sum: " + to_string(fn_sum))
// sum(3 * i) for i=1..500 = 3 * 125250 = 375750
if (fn_sum == 375750) {
    print("  PASS")
} else {
    print("  FAIL: expected 375750 got " + to_string(fn_sum))
}

print("\n=== GC Stress Test Complete ===")
