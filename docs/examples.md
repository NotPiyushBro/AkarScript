# Examples

## Hello World

```akar
print("Hello, Akar!")
```

## Variables and Types

```akar
let name = "Akar"
let version = 0.1
let active = true
let nothing = nil

print(name + " v" + to_string(version))
print("Active: " + to_string(active))
```

## FizzBuzz

```akar
for i in 1..100 {
    if (i % 15 == 0) {
        print("FizzBuzz")
    } else if (i % 3 == 0) {
        print("Fizz")
    } else if (i % 5 == 0) {
        print("Buzz")
    } else {
        print(i)
    }
}
```

## Fibonacci

```akar
fn fib(n) {
    if (n <= 1) return n
    return fib(n - 1) + fib(n - 2)
}

for i in 0..20 {
    print("fib(" + to_string(i) + ") = " + to_string(fib(i)))
}
```

## Closures and Counters

```akar
fn make_counter(start) {
    let count = start
    return fn() {
        count = count + 1
        return count
    }
}

let counter = make_counter(0)
print(counter())  // 1
print(counter())  // 2
print(counter())  // 3
```

## Higher-Order Functions

```akar
fn apply(f, x) {
    return f(x)
}

fn double(x) { x * 2 }
fn square(x) { x * x }

print(apply(double, 5))  // 10
print(apply(square, 5))  // 25
```

## Array Processing

```akar
let data = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3]

// Find max
let max_val = data[0]
for item in data {
    if (item > max_val) {
        max_val = item
    }
}
print("Max: " + to_string(max_val))

// Sum
let total = 0
for item in data {
    total = total + item
}
print("Sum: " + to_string(total))
```

## Map as Struct

```akar
let player = {
    "name": "Alice",
    "hp": 100,
    "x": 0,
    "y": 0,
    "inventory": ["sword", "shield", "potion"]
}

print(player.name + " has " + to_string(len(player.inventory)) + " items")
```

## Classes: Vec2 with Operator Overloading

```akar
class Vec2 {
    init(x, y) {
        this.x = x
        this.y = y
    }

    __add(o) { return Vec2(this.x + o.x, this.y + o.y) }
    __sub(o) { return Vec2(this.x - o.x, this.y - o.y) }
    __mul(s) { return Vec2(this.x * s, this.y * s) }
    __neg()  { return Vec2(-this.x, -this.y) }
    __eq(o)  { return this.x == o.x and this.y == o.y }

    len()      { return sqrt(this.x * this.x + this.y * this.y) }
    normalize() { let l = this.len(); return Vec2(this.x / l, this.y / l) }
    dot(o)     { return this.x * o.x + this.y * o.y }
    dist(o)    { return (this - o).len() }

    str() { return "Vec2(" + to_string(this.x) + "," + to_string(this.y) + ")" }
}

let a = Vec2(3, 4)
let b = Vec2(1, 2)

print("a = " + a.str())
print("b = " + b.str())
print("a + b = " + (a + b).str())
print("a * 2 = " + (a * 2).str())
print("|a| = " + to_string(a.len()))
print("a·b = " + to_string(a.dot(b)))
```

## Classes: Game Entity

```akar
class Entity {
    init(name, hp, x, y) {
        this.name = name
        this.hp = hp
        this.max_hp = hp
        this.x = x
        this.y = y
        this.alive = true
    }

    take_damage(amount) {
        this.hp = this.hp - amount
        if (this.hp <= 0) {
            this.hp = 0
            this.alive = false
        }
    }

    heal(amount) {
        this.hp = min(this.hp + amount, this.max_hp)
    }

    move(dx, dy) {
        this.x = this.x + dx
        this.y = this.y + dy
    }

    distance_to(other) {
        let dx = this.x - other.x
        let dy = this.y - other.y
        return sqrt(dx * dx + dy * dy)
    }

    str() {
        return this.name + " HP:" + to_string(this.hp) + "/" + to_string(this.max_hp) +
               " (" + to_string(this.x) + "," + to_string(this.y) + ")"
    }
}

let player = Entity("Hero", 100, 0, 0)
let enemy = Entity("Goblin", 30, 5, 5)

print(player.str())
print(enemy.str())
print("Distance: " + to_string(player.distance_to(enemy)))

enemy.take_damage(15)
print(enemy.str())
```

## Classes: State Machine

```akar
class StateMachine {
    init(initial) {
        this.state = initial
        this.transitions = {}
    }

    add_transition(from_state, event, to_state) {
        if (this.transitions[from_state] == nil) {
            this.transitions[from_state] = {}
        }
        this.transitions[from_state][event] = to_state
    }

    handle(event) {
        let trans = this.transitions[this.state]
        if (trans != nil and trans[event] != nil) {
            let old = this.state
            this.state = trans[event]
            print(old + " -> " + this.state + " (on " + event + ")")
        }
    }

    str() { return "State: " + this.state }
}

let fsm = StateMachine("idle")
fsm.add_transition("idle", "start", "running")
fsm.add_transition("running", "pause", "paused")
fsm.add_transition("paused", "resume", "running")
fsm.add_transition("running", "stop", "idle")
fsm.add_transition("paused", "stop", "idle")

fsm.handle("start")   // idle -> running
fsm.handle("pause")   // running -> paused
fsm.handle("resume")  // paused -> running
fsm.handle("stop")    // running -> idle
```

## Fibers: Producer-Consumer

```akar
fn producer() {
    let i = 0
    while (true) {
        fiber_yield(i * i)
        i = i + 1
    }
}

fn consumer(prod_fiber, count) {
    let total = 0
    for i in 0..count - 1 {
        let val = fiber_resume(prod_fiber)
        total = total + val
        print("Got: " + to_string(val))
    }
    return total
}

let prod = fiber_create(producer)
let sum = consumer(prod, 5)
print("Total: " + to_string(sum))
// Got: 0, 1, 4, 9, 16 → Total: 30
```

## Fibers: Pipeline

```akar
fn range_gen(start, end) {
    let i = start
    while (i <= end) {
        fiber_yield(i)
        i = i + 1
    }
}

fn map_gen(src, fn_transform) {
    while (true) {
        let v = fiber_resume(src)
        if (fiber_status(src) == "done") break
        fiber_yield(fn_transform(v))
    }
}

fn filter_gen(src, fn_predicate) {
    while (true) {
        let v = fiber_resume(src)
        if (fiber_status(src) == "done") break
        if (fn_predicate(v)) fiber_yield(v)
    }
}

fn collect(f) {
    let result = []
    while (true) {
        let v = fiber_resume(f)
        if (fiber_status(f) == "done") break
        push(result, v)
    }
    return result
}

// Pipeline: 1..20 → multiply by 3 → keep even → collect
let source = fiber_create(range_gen, 1, 20)
let mapped = fiber_create(map_gen, source, fn(x) { x * 3 })
let filtered = fiber_create(filter_gen, mapped, fn(x) { x % 2 == 0 })
let result = collect(filtered)

print(result)  // [6, 12, 18, 24, 30, 36, 42, 48, 54, 60]
```

## Error Handling

```akar
fn divide(a, b) {
    if (b == 0) {
        throw "Division by zero"
    }
    return a / b
}

try {
    let result = divide(10, 0)
    print("Result: " + to_string(result))
} catch (e) {
    print("Error caught: " + to_string(e))
}
```

## Physics Simulation

```akar
class Vec2 {
    init(x, y) { this.x = x  this.y = y }
    __add(o) { return Vec2(this.x + o.x, this.y + o.y) }
    __sub(o) { return Vec2(this.x - o.x, this.y - o.y) }
    __mul(s) { return Vec2(this.x * s, this.y * s) }
    len() { return sqrt(this.x * this.x + this.y * this.y) }
    str() { return "(" + to_string(this.x) + "," + to_string(this.y) + ")" }
}

class Particle {
    init(x, y, vx, vy) {
        this.pos = Vec2(x, y)
        this.vel = Vec2(vx, vy)
    }

    update(dt, gravity) {
        this.vel = this.vel + gravity * dt
        this.pos = this.pos + this.vel * dt
    }
}

let gravity = Vec2(0, -9.81)
let ball = Particle(0, 100, 10, 0)

let dt = 0.1
for i in 1..20 {
    ball.update(dt, gravity)
    print("t=" + to_string(i * dt) + " pos=" + ball.pos.str())
}
```

## Embedding Example (C++)

```cpp
#include "akar/api/akar.h"
#include <cstdio>

static int game_add_score(akar_VM* vm, int argc, int argv_base) {
    double current = akar_to_number(vm, argv_base);
    double points = akar_to_number(vm, argv_base + 1);
    akar_push_number(vm, current + points);
    return 1;
}

int main() {
    akar_VM* vm = akar_new_vm();
    
    // Register game function
    akar_register(vm, "add_score", game_add_score);
    
    // Set initial state
    akar_push_number(vm, 0);
    akar_set_global(vm, "score");
    
    // Run game logic
    akar_exec(vm, R"(
        fn on_enemy_killed(points) {
            score = add_score(score, points)
            print("Score: " + to_string(score))
        }
        
        on_enemy_killed(100)
        on_enemy_killed(250)
        on_enemy_killed(50)
    )");
    
    // Read final score
    akar_get_global(vm, "score");
    printf("Final score: %.0f\n", akar_to_number(vm, -1));
    akar_pop(vm, 1);
    
    akar_free_vm(vm);
    return 0;
}
```

## Benchmark: Sum of Primes

```akar
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
print("Sum: " + to_string(sum))
// Primes found: 9592
// Sum: 454396537
```
