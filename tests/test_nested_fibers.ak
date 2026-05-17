fn inner(initial) {
    print("  Inner: initial=" + to_string(initial))
    print("  Inner: yielding 1")
    let v = fiber_yield(1)
    print("  Inner: received " + to_string(v))
    return 2
}

fn outer(initial) {
    print("Outer: initial=" + to_string(initial))
    print("Outer: creating inner")
    let f_inner = fiber_create(inner)
    print("Outer: resuming inner")
    let r1 = fiber_resume(f_inner, "a")
    print("Outer: inner yielded " + to_string(r1))
    
    print("Outer: yielding 3")
    let v2 = fiber_yield(3)
    print("Outer: received " + to_string(v2))
    return 4
}

print("Main: creating outer")
let f_outer = fiber_create(outer)
print("Main: resuming outer")
let r1 = fiber_resume(f_outer, "x")
print("Main: outer yielded " + to_string(r1))
print("Main: resuming outer again")
let r2 = fiber_resume(f_outer, "y")
print("Main: outer returned " + to_string(r2))
