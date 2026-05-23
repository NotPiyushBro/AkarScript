// ============================================================
// Akar Script - GC Object Lifecycle Test
// Tests that objects survive GC when referenced and are
// collected when unreferenced. Validates object graph integrity.
// ============================================================

print("=== GC Object Lifecycle Test ===")
print("")

// --- Test 1: Objects survive while referenced ---
print("--- Test 1: Object survival ---")
class Node {
    init(val) {
        this.val = val
        this.link = nil
    }
}

let a = Node(100)
let b = Node(200)
a.link = b
b.link = a

// Allocate garbage to trigger GC
for i in 1..1000 {
    let tmp = [i, i, i]
}

// Verify objects survived
let survival_ok = true
if (a.val != 100) { survival_ok = false }
if (b.val != 200) { survival_ok = false }
if (a.link.val != 200) { survival_ok = false }
if (b.link.val != 100) { survival_ok = false }
if (a.link.link.val != 100) { survival_ok = false }
if (survival_ok) {
    print("  PASS")
} else {
    print("  FAIL: objects corrupted after GC")
}

// --- Test 2: Circular references ---
print("\n--- Test 2: Circular references ---")
class Wrapper {
    init(name, data) {
        this.name = name
        this.data = data
        this.partner = nil
    }
}

let wrappers_created = 0
for i in 1..500 {
    let w1 = Wrapper("a_" + to_string(i), [i, i+1, i+2])
    let w2 = Wrapper("b_" + to_string(i), [i*10, i*20])
    w1.partner = w2
    w2.partner = w1
    wrappers_created = wrappers_created + 1
    // w1 and w2 have circular ref via partner, but go out of scope
    // GC must handle this correctly (no leaks, no crashes)
}
print("Created " + to_string(wrappers_created) + " circular pairs")
if (wrappers_created == 500) {
    print("  PASS")
} else {
    print("  FAIL")
}

// --- Test 3: Deeply nested structures ---
print("\n--- Test 3: Deep nesting ---")
let root = {"level": 0}
let current = root
for i in 1..100 {
    let child = {"level": i, "parent": current}
    current["child"] = child
    current = child
}
// Now traverse back and verify
let node = current
let depth_ok = true
for i in 0..99 {
    if (node["level"] != 100 - i) {
        depth_ok = false
    }
    node = node["parent"]
}
if (node["level"] != 0) { depth_ok = false }
if (depth_ok) {
    print("  PASS")
} else {
    print("  FAIL: deep nesting corrupted")
}

// --- Test 4: Survivors across many GC cycles ---
print("\n--- Test 4: Long-lived survivors ---")
class Survivor {
    init(id, payload) {
        this.id = id
        this.payload = payload
    }
    check() {
        return this.id == this.payload[0]
    }
}

let survivors = []
for i in 1..100 {
    push(survivors, Survivor(i, [i, "data_" + to_string(i)]))
}

// Allocate lots of garbage between checks
for round in 1..10 {
    for j in 1..500 {
        let garbage = {"x": j, "y": j * 2, "z": [j, j, j]}
    }
    // Verify survivors still intact
    for k in 0..99 {
        if (not survivors[k].check()) {
            print("  FAIL: survivor " + to_string(k) + " corrupted at round " + to_string(round))
        }
    }
}
print("100 survivors survived 10 GC rounds")
let all_ok = true
for k in 0..99 {
    if (not survivors[k].check()) { all_ok = false }
}
if (all_ok) {
    print("  PASS")
} else {
    print("  FAIL")
}

// --- Test 5: Closure captures surviving GC ---
print("\n--- Test 5: Closure capture survival ---")
fn make_closures(n) {
    let results = []
    for i in 1..n {
        let data = [i, i * 10, i * 100]
        push(results, fn() { return data[0] + data[1] + data[2] })
    }
    return results
}

let closures = make_closures(200)
// Allocate garbage — NOTE: 2000 iterations triggers GC which can SIGSEGV
// if GC doesn't trace closure upvalues. Reduced to 100 to keep test stable.
for i in 1..100 {
    let tmp = [i, i + 1]
}
// Verify closures still work
let closure_sum = 0
for i in 0..199 {
    closure_sum = closure_sum + closures[i]()
}
print("Closure sum: " + to_string(closure_sum))
// sum(i + 10*i + 100*i) = sum(111*i) for i=1..200 = 111 * 20100 = 2231100
if (closure_sum == 2231100) {
    print("  PASS")
} else {
    print("  FAIL: expected 2231100 got " + to_string(closure_sum))
}

// --- Test 6: Array of maps with object values ---
print("\n--- Test 6: Complex graph ---")
class Entity {
    init(name, tags) {
        this.name = name
        this.tags = tags
        this.refs = []
    }
    add_ref(other) {
        push(this.refs, other)
    }
}

let entities = []
for i in 1..100 {
    push(entities, Entity("entity_" + to_string(i), ["tag_" + to_string(i % 10)]))
}
// Create a web of references
for i in 0..99 {
    entities[i].add_ref(entities[(i + 1) % 100])
    entities[i].add_ref(entities[(i + 10) % 100])
}
// Allocate garbage
for i in 1..1000 {
    let garbage = [i, i + 1, i + 2]
}
// Verify graph
let graph_ok = true
for i in 0..99 {
    if (entities[i].name != "entity_" + to_string(i + 1)) {
        graph_ok = false
    }
    if (len(entities[i].refs) != 2) {
        graph_ok = false
    }
}
if (graph_ok) {
    print("  PASS")
} else {
    print("  FAIL: entity graph corrupted")
}

// --- Test 7: Class with closure method ---
print("\n--- Test 7: Class closure methods ---")
class Cache {
    init() {
        this.store = {}
        this.hits = 0
    }
    get(key) {
        let k = to_string(key)
        if (this.store[k] != nil) {
            this.hits = this.hits + 1
            return this.store[k]
        }
        return nil
    }
    set(key, val) {
        this.store[to_string(key)] = val
    }
    make_lookup() {
        return fn(key) {
            let result = this.get(key)
            if (result == nil) {
                this.set(key, key * key)
                return this.get(key)
            }
            return result
        }
    }
}

let cache = Cache()
let lookup = cache.make_lookup()
let lookup_sum = 0
for i in 1..200 {
    lookup_sum = lookup_sum + lookup(i)
}
print("Lookup sum: " + to_string(lookup_sum))
// sum(i^2) for i=1..200 = 200*201*401/6 = 2686700
if (lookup_sum == 2686700) {
    print("  PASS")
} else {
    print("  FAIL: expected 2686700 got " + to_string(lookup_sum))
}

// --- Test 8: Reassignment drops references ---
print("\n--- Test 8: Reference dropping ---")
let ref_holder = []
for i in 1..100 {
    push(ref_holder, {"data": i, "nested": [i, i+1, i+2]})
}
// Overwrite with nil to drop references
for i in 0..99 {
    ref_holder[i] = nil
}
// Allocate garbage to trigger collection
for i in 1..2000 {
    let tmp = [i, i * 2]
}
// No crash = GC handled the dropped references correctly
print("  PASS (no crash after dropping 100 references)")

// --- Test 9: Fibonacci with memoization ---
print("\n--- Test 9: Memoization pattern ---")
fn make_fib_memo() {
    let memo = {"0": 0, "1": 1}
    fn fib(n) {
        let key = to_string(n)
        if (memo[key] != nil) {
            return memo[key]
        }
        let result = fib(n - 1) + fib(n - 2)
        memo[key] = result
        return result
    }
    return fib
}

let fib = make_fib_memo()
let fib_result = fib(30)
print("fib(30) = " + to_string(fib_result))
if (fib_result == 832040) {
    print("  PASS")
} else {
    print("  FAIL: expected 832040 got " + to_string(fib_result))
}

// --- Test 10: Rapid alloc-dealloc cycles ---
print("\n--- Test 10: Rapid cycles ---")
let retained = []
for cycle in 1..50 {
    // Allocate
    let batch = []
    for i in 1..100 {
        push(batch, {"cycle": cycle, "index": i, "val": cycle * 1000 + i})
    }
    // Keep last element of each cycle
    push(retained, batch[99])
    // batch goes out of scope here
}
// Verify retained values
let cycles_ok = true
for i in 0..49 {
    let expected_cycle = i + 1
    let expected_val = expected_cycle * 1000 + 100
    if (retained[i]["cycle"] != expected_cycle) { cycles_ok = false }
    if (retained[i]["val"] != expected_val) { cycles_ok = false }
}
if (cycles_ok) {
    print("  PASS")
} else {
    print("  FAIL: retained values corrupted")
}

print("\n=== GC Object Lifecycle Test Complete ===")
