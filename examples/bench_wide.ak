// Benchmark: closure and field access heavy workloads
// Tests CLOSURE, CLOSE_UPVALUE, GET_FIELD, SET_FIELD paths in JIT

fn test_closure_calls(n) {
    fn make_adder(x) {
        fn adder(y) {
            return x + y
        }
        return adder
    }
    
    let total = 0
    let i = 0
    while (i < n) {
        let add10 = make_adder(10)
        let j = 0
        while (j < 100) {
            total = total + add10(j)
            j = j + 1
        }
        i = i + 1
    }
    return total
}

class Vec {
    init(x, y, z) {
        this.x = x
        this.y = y
        this.z = z
    }
}

fn test_field_access(n) {
    let obj = Vec(0, 0, 0)
    let i = 0
    while (i < n) {
        obj.x = obj.x + 1
        obj.y = obj.y + 2
        obj.z = obj.z + obj.x + obj.y
        i = i + 1
    }
    return obj.z
}

fn test_tail_call(n) {
    fn countdown(n, acc) {
        if (n <= 0) { return acc }
        return countdown(n - 1, acc + 1)
    }
    let i = 0
    while (i < n) {
        countdown(100, 0)
        i = i + 1
    }
    return 0
}

print("=== Closure & Field Benchmark ===")
print("Each function called 5000+ times (JIT triggers at 50)")

test_closure_calls(5000)
print("closure_calls(5000): done")

test_field_access(50000)
print("field_access(50000): done")

test_tail_call(5000)
print("tail_call(5000): done")

print("Done!")
