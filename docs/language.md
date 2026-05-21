# Language Reference

## Overview

Akar is a dynamically typed, garbage-collected scripting language with:
- Register-based bytecode VM
- First-class functions and closures
- Classes with methods and operator overloading
- Fibers (coroutines) for cooperative multitasking
- Incremental mark-sweep garbage collector

## Comments

```akar
// Single-line comment

/* 
   Multi-line comments are not supported.
   Use // on each line.
*/
```

## Variables

Variables are declared with `let`:

```akar
let x = 10
let name = "hello"
let flag = true
let nothing = nil
```

Variables are mutable — reassign with `=`:

```akar
let x = 10
x = 20  // ok
```

Without `let`, assignment targets an existing variable:

```akar
let x = 10
x = x + 1  // modifies existing x
y = 5       // error: undefined variable 'y'
```

### Destructuring

```akar
let [a, b, c] = [10, 20, 30]
print(a)  // 10
print(b)  // 20
print(c)  // 30
```

## Types

Akar has these types:

| Type | Example | Description |
|------|---------|-------------|
| `number` | `42`, `3.14`, `-7` | IEEE 754 double |
| `string` | `"hello"`, `'world'` | UTF-8 strings (interned) |
| `bool` | `true`, `false` | Boolean |
| `nil` | `nil` | Null/absence |
| `array` | `[1, 2, 3]` | Ordered collection |
| `map` | `{"a": 1, "b": 2}` | String-keyed map |
| `function` | `fn(x) { x * 2 }` | First-class function |
| `class` | `class Foo { }` | Class definition |
| `instance` | `Foo()` | Class instance |
| `fiber` | `fiber_create(fn)` | Coroutine |
| `signal` | `signal x = 10` | Reactive variable |
| `effect` | `effect { ... }` | Reactive side-effect block |
| `enum value` | `Color.Red` | NaN-boxed enum variant |

### Type Checking

```akar
type(42)         // "number"
type("hello")    // "string"
type(true)       // "bool"
type(nil)        // "nil"
type([1,2])      // "array"
type({})         // "map"
type(fn() {})    // "function"
```

## Numbers

```akar
let integer = 42
let float = 3.14159
let negative = -7
let hex = 0xFF    // not supported, use 255
```

Arithmetic:

```akar
10 + 3     // 13     addition
10 - 3     // 7      subtraction
10 * 3     // 30     multiplication
10 / 3     // 3.333  division
10 % 3     // 1      modulo
-5         // -5     negation
```

Comparison:

```akar
5 == 5     // true
5 != 3     // true
5 < 10     // true
5 <= 5     // true
10 > 5     // true
10 >= 10   // true
```

## Strings

```akar
let s1 = "double quotes"
let s2 = 'single quotes'
let s3 = "escape\nsequences\twork"
```

Concatenation:

```akar
let greeting = "hello" + " " + "world"
```

String functions:

```akar
len("hello")           // 5
to_string(42)          // "42"
to_number("3.14")      // 3.14
contains("hello", "ell")  // true
replace("hello", "l", "r")  // "herro"
split("a,b,c", ",")    // ["a", "b", "c"]
join(["a","b"], "-")   // "a-b"
substr("hello", 1, 3)  // "ell"
ascii("A")             // 65
char(65)               // "A"
format("{} is {}", "life", 42)  // "life is 42"
concat("a", "b", "c")  // "abc"
```

## Booleans

```akar
let t = true
let f = false
```

Logical operators:

```akar
true and true     // true
true and false    // false
true or false     // true
false or false    // false
not true          // false
not false         // true
```

Truthy values: everything except `nil` and `false`.

```akar
if (0) print("yes")    // prints "yes" (0 is truthy!)
if ("") print("yes")   // prints "yes" (empty string is truthy!)
if (nil) print("yes")  // does NOT print
if (false) print("yes") // does NOT print
```

## Arrays

```akar
let arr = [1, 2, 3, "hello", true]
```

Access:

```akar
arr[0]       // 1 (first element)
arr[4]       // true (last element)
arr[-1]      // true (negative indexing)
arr[99]      // error: index out of bounds
```

Mutation:

```akar
arr[0] = 99  // modify element
push(arr, 42)  // add to end
pop(arr)       // remove and return last
```

Array functions:

```akar
len(arr)         // 5
push(arr, x)     // add to end, returns array
pop(arr)         // remove last, returns value
contains(arr, x) // check if element exists
range(0, 10)     // [0, 1, 2, ..., 10]
range(0, 10, 2)  // [0, 2, 4, 6, 8, 10]
```

Iteration:

```akar
for item in arr {
    print(item)
}

for i in 0..len(arr) - 1 {
    print(arr[i])
}
```

## Maps

```akar
let m = {"name": "Alice", "age": 30, "active": true}
```

Access:

```akar
m["name"]    // "Alice"
m.name       // "name" syntax (same as above)
m.unknown    // nil (missing keys return nil)
```

Mutation:

```akar
m["score"] = 100
m.score = 100     // same thing
```

Map functions:

```akar
keys(m)       // ["name", "age", "active", "score"]
values(m)     // ["Alice", 30, true, 100]
len(m)        // 4
contains(m, "name")  // true
```

Iteration:

```akar
for key in keys(m) {
    print(key + ": " + to_string(m[key]))
}
```

## Control Flow

### If/Else

```akar
if (x > 0) {
    print("positive")
} else if (x < 0) {
    print("negative")
} else {
    print("zero")
}
```

Single-line (no braces needed for single statement):

```akar
if (x > 0) print("positive")
```

### While

```akar
let i = 0
while (i < 10) {
    print(i)
    i = i + 1
}
```

### For (C-style)

```akar
for (let i = 0; i < 10; i = i + 1) {
    print(i)
}
```

### For-In

```akar
// Range
for i in 0..10 {
    print(i)  // 0, 1, 2, ..., 10 (inclusive)
}

// Array
for item in [1, 2, 3] {
    print(item)
}

// String
for ch in "hello" {
    print(ch)
}
```

### Break and Continue

```akar
for i in 0..100 {
    if (i == 5) continue  // skip 5
    if (i == 8) break     // stop at 8
    print(i)  // 0, 1, 2, 3, 4, 6, 7
}
```

### Switch

```akar
switch (value) {
    case 1, 2, 3:
        print("small")
    case 10:
        print("ten")
    default:
        print("other")
}
```

## Functions

### Named Functions

```akar
fn add(a, b) {
    return a + b
}
let result = add(3, 4)  // 7
```

### Implicit Return

The last expression in a function body is automatically returned:

```akar
fn add(a, b) {
    a + b  // implicit return
}
```

### Variadic Functions

```akar
fn sum(...args) {
    let total = 0
    for a in args {
        total = total + a
    }
    return total
}
sum(1, 2, 3, 4)  // 10
```

### Lambdas

```akar
let double = fn(x) { x * 2 }
double(5)  // 10

let arr = [1, 2, 3]
// (no built-in map, but you can do:)
let result = []
for item in arr {
    push(result, item * 2)
}
```

### Closures

Functions capture variables from enclosing scopes:

```akar
fn make_counter(start) {
    let count = start
    return fn() {
        count = count + 1
        return count
    }
}
let counter = make_counter(0)
counter()  // 1
counter()  // 2
counter()  // 3
```

### Recursive Functions

```akar
fn factorial(n) {
    if (n <= 1) return 1
    return n * factorial(n - 1)
}
factorial(10)  // 3628800
```

Local recursive functions:

```akar
fn make_fib() {
    fn fib(n) {
        if (n <= 1) return n
        return fib(n - 1) + fib(n - 2)
    }
    return fib
}
let fib = make_fib()
fib(10)  // 55
```

## Classes

### Basic Class

```akar
class Point {
    init(x, y) {
        this.x = x
        this.y = y
    }

    distance_to(other) {
        let dx = this.x - other.x
        let dy = this.y - other.y
        return sqrt(dx * dx + dy * dy)
    }

    str() {
        return "Point(" + to_string(this.x) + ", " + to_string(this.y) + ")"
    }
}

let p = Point(3, 4)
print(p.x)              // 3
print(p.distance_to(Point(0, 0)))  // 5
print(p.str())          // "Point(3, 4)"
```

### Inheritance

Not yet supported. Use composition instead:

```akar
class Shape {
    init(type) { this.type = type }
}

class Circle {
    init(x, y, r) {
        this.shape = Shape("circle")
        this.x = x
        this.y = y
        this.r = r
    }
}
```

### Operator Overloading

See [Operator Overloading](operators.md) for full details.

```akar
class Vec2 {
    init(x, y) { this.x = x  this.y = y }
    __add(o) { return Vec2(this.x + o.x, this.y + o.y) }
    __mul(s) { return Vec2(this.x * s, this.y * s) }
    __eq(o)  { return this.x == o.x and this.y == o.y }
}

Vec2(3, 4) + Vec2(1, 2)  // Vec2(4, 6)
Vec2(3, 4) * 2            // Vec2(6, 8)
```

## Error Handling

### Try/Catch

```akar
try {
    let x = 10 / 0
} catch (e) {
    print("Error: " + to_string(e))
}
```

### Throw

```akar
fn validate(x) {
    if (x < 0) {
        throw "Value must be positive"
    }
    return x
}

try {
    validate(-1)
} catch (e) {
    print(e)  // "Value must be positive"
}
```

## Include

Import other `.ak` files:

```akar
include "math_utils.ak"
include "lib/helpers.ak"
```

Paths are relative to the including file's directory.

## Await

Suspend execution until a value is available:

```akar
let result = await some_async_value
```

If the value is `nil`, execution suspends (used with fibers).

## Enums

Enums define a type with a fixed set of named variants. Simple variants are NaN-boxed into the value itself — no heap allocation, no GC pressure, instant comparison.

### Simple Enums

```akar
enum Direction {
    North,
    South,
    East,
    West
}

let dir = Direction.North
print(dir)                    // <enum #0:0>
print(dir == Direction.North) // true
print(dir == Direction.South) // false
```

### Multiple Enum Types

Each enum type gets a unique internal ID. Different types are always unequal:

```akar
enum Color { Red, Green, Blue }
enum Priority { Low, Medium, High }

let c = Color.Red
let p = Priority.High
print(c == p)  // false — different types, even though both are "first" variant
```

### Enums in Control Flow

```akar
enum GameState { Menu, Playing, Paused, GameOver }

let state = GameState.Menu

if (state == GameState.Playing) {
    print("Game is running")
} else if (state == GameState.Paused) {
    print("Game is paused")
}
```

### Enums with Signals

Enums are often used with signals for reactive state machines:

```akar
enum GameState { Menu, Playing, Paused, GameOver }
signal state = GameState.Menu

effect {
    if (state == GameState.Menu) {
        show_menu()
    } else if (state == GameState.Playing) {
        hide_menu()
    } else if (state == GameState.GameOver) {
        show_death_screen()
    }
}

state = GameState.Playing  // automatically hides menu
```

### Performance

- Simple enum values are **8 bytes, NaN-boxed** — no heap allocation
- Comparison is a single `bits == bits` check (faster than string comparison)
- Zero GC pressure — enum values are immediate, not heap objects

## Reactive Signals & Effects

See [Reactive Signals & Effects](reactive.md) for full documentation.

```akar
signal health = 100

effect {
    print("Health: " + to_string(health))
}

health = 80   // automatically prints "Health: 80"
```

## Profiling & Tracing

See [Profiling & Tracing](profiling.md) for full documentation.

```akar
profile_start()
// ... your code ...
profile_report()
```

## Fibers (Coroutines)

See [Standard Library - Fibers](stdlib.md#fibers) for details.

```akar
fn counter() {
    let i = 0
    while (true) {
        fiber_yield(i)
        i = i + 1
    }
}

let f = fiber_create(counter)
fiber_resume(f)  // 0
fiber_resume(f)  // 1
fiber_resume(f)  // 2
```

## Operator Precedence

From highest to lowest:

| Precedence | Operators |
|-----------|-----------|
| 1 (highest) | `()`, `[]`, `.` (call, index, field access) |
| 2 | `-`, `!`, `not` (unary) |
| 3 | `*`, `/`, `%` |
| 4 | `+`, `-` |
| 5 | `..` (range) |
| 6 | `<`, `<=`, `>`, `>=` |
| 7 | `==`, `!=` |
| 8 | `and`, `&&` |
| 9 | `or`, `\|\|` |
| 10 (lowest) | `=` (assignment) |

## Keywords

```
let fn if else while for in return break continue
class this super new and or not include await
switch case default try catch throw true false nil
signal effect enum
```
