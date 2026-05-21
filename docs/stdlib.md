# Standard Library

Akar includes a comprehensive set of built-in functions available in all scripts.

## Output

### `print(...args)`

Prints arguments separated by spaces, followed by a newline.

```akar
print("hello")              // hello
print("a", "b", "c")       // a b c
print(42)                   // 42
print("x=" + to_string(x)) // x=10
```

## Type Conversion

### `to_string(value)`

Converts any value to its string representation.

```akar
to_string(42)       // "42"
to_string(3.14)     // "3.14"
to_string(true)     // "true"
to_string(nil)      // "nil"
to_string([1,2])    // "[1, 2]"
```

### `to_number(value)`

Converts a string or bool to a number. Returns `nil` on failure.

```akar
to_number("42")     // 42
to_number("3.14")   // 3.14
to_number(true)     // 1
to_number("abc")    // nil
to_number("")       // nil
```

### `int(value)`

Truncates to integer (toward zero).

```akar
int(3.7)    // 3
int(-3.7)   // -3
int("42")   // 42
int(true)   // 1
```

### `type(value)`

Returns the type name as a string.

```akar
type(42)       // "number"
type("hi")     // "string"
type(true)     // "bool"
type(nil)      // "nil"
type([1])      // "array"
type({})       // "map"
type(fn(){})   // "function"
```

## Collections

### `len(collection)`

Returns the length of a string, array, or map.

```akar
len("hello")    // 5
len([1,2,3])    // 3
len({"a": 1})   // 1
```

### `push(array, value)`

Appends `value` to the end of `array`. Returns the array.

```akar
let arr = [1, 2]
push(arr, 3)    // arr is now [1, 2, 3]
```

### `pop(array)`

Removes and returns the last element. Returns `nil` if empty.

```akar
let arr = [1, 2, 3]
pop(arr)    // 3, arr is now [1, 2]
```

### `contains(collection, key)`

Checks if an array contains a value, a string contains a substring, or a map has a key.

```akar
contains([1, 2, 3], 2)       // true
contains("hello", "ell")     // true
contains({"a": 1}, "a")      // true
contains({"a": 1}, "b")      // false
```

### `keys(map)` / `values(map)`

Returns arrays of map keys or values.

```akar
let m = {"a": 1, "b": 2}
keys(m)     // ["a", "b"]
values(m)   // [1, 2]
```

## String Functions

### `split(str, delimiter)`

Splits a string by delimiter into an array.

```akar
split("a,b,c", ",")     // ["a", "b", "c"]
split("hello", "")       // ["h", "e", "l", "l", "o"]
split("abc", "b")       // ["a", "c"]
```

### `join(array, delimiter)`

Joins array elements into a string.

```akar
join(["a", "b", "c"], "-")  // "a-b-c"
join([1, 2, 3], ", ")       // "1, 2, 3"
```

### `replace(str, old, new)`

Replaces all occurrences of `old` with `new`.

```akar
replace("hello world", "world", "akar")  // "hello akar"
replace("aaa", "a", "b")                // "bbb"
```

### `substr(str, start, length?)`

Returns a substring. `length` is optional (defaults to rest of string).

```akar
substr("hello", 1, 3)   // "ell"
substr("hello", 2)      // "llo"
substr("hello", 0, 1)   // "h"
```

### `ascii(str)` / `char(code)`

Convert between characters and ASCII codes.

```akar
ascii("A")     // 65
char(65)       // "A"
ascii("hello") // 104 (first char)
```

### `concat(...args)`

Concatenates all arguments as strings. Faster than `+` in loops.

```akar
concat("a", "b", "c")  // "abc"
concat("x=", 42)        // "x=42"
```

### `format(template, ...args)`

Replaces `{}` placeholders with arguments.

```akar
format("{} is {}", "life", 42)      // "life is 42"
format("x={}, y={}", 10, 20)        // "x=10, y=20"
format("{{escaped}}")               // "{escaped}"
```

## Math

### Constants

| Name | Value | Description |
|------|-------|-------------|
| `PI` | 3.14159... | π |
| `E` | 2.71828... | Euler's number |

### Arithmetic

| Function | Args | Description |
|----------|------|-------------|
| `abs(x)` | 1 | Absolute value |
| `sqrt(x)` | 1 | Square root |
| `pow(base, exp)` | 2 | Power |
| `exp(x)` | 1 | e^x |
| `log(x)` | 1 | Natural logarithm |
| `log2(x)` | 1 | Base-2 logarithm |
| `log10(x)` | 1 | Base-10 logarithm |
| `fmod(x, y)` | 2 | Floating-point remainder |

### Rounding

| Function | Description |
|----------|-------------|
| `floor(x)` | Round toward -∞ |
| `ceil(x)` | Round toward +∞ |
| `round(x)` | Round to nearest |
| `trunc(x)` | Round toward 0 |

### Comparison

| Function | Args | Description |
|----------|------|-------------|
| `min(a, b, ...)` | 2+ | Minimum (variadic) |
| `max(a, b, ...)` | 2+ | Maximum (variadic) |
| `clamp(val, lo, hi)` | 3 | Clamp to range |
| `sign(x)` | 1 | Returns -1, 0, or 1 |
| `lerp(a, b, t)` | 3 | Linear interpolation |

### Trigonometry

| Function | Description |
|----------|-------------|
| `sin(x)` | Sine (radians) |
| `cos(x)` | Cosine |
| `tan(x)` | Tangent |
| `asin(x)` | Arc sine |
| `acos(x)` | Arc cosine |
| `atan(x)` | Arc tangent |
| `atan2(y, x)` | Two-argument arc tangent |
| `sinh(x)` | Hyperbolic sine |
| `cosh(x)` | Hyperbolic cosine |
| `tanh(x)` | Hyperbolic tangent |
| `deg_to_rad(deg)` | Degrees → radians |
| `rad_to_deg(rad)` | Radians → degrees |

### Special Values

| Function | Description |
|----------|-------------|
| `nan()` | Returns NaN |
| `inf()` | Returns infinity |
| `isnan(x)` | Check if NaN |
| `isinf(x)` | Check if infinite |

### Random

```akar
random()     // Random float in [0, 1)
```

## Vec2 (2D Vectors)

Vec2 values are arrays `[x, y]`.

| Function | Description |
|----------|-------------|
| `vec2(x, y)` | Create Vec2 |
| `vec2_add(a, b)` | Element-wise add |
| `vec2_sub(a, b)` | Element-wise subtract |
| `vec2_scale(v, s)` | Multiply by scalar |
| `vec2_dot(a, b)` | Dot product |
| `vec2_len(v)` | Magnitude |
| `vec2_normalize(v)` | Unit vector |
| `vec2_dist(a, b)` | Distance |

```akar
let a = vec2(3, 4)
let b = vec2(1, 2)
vec2_add(a, b)       // [4, 6]
vec2_len(a)          // 5
vec2_dot(a, b)       // 11
vec2_normalize(a)    // [0.6, 0.8]
vec2_dist(a, b)      // 2.828...
```

## Vec3 (3D Vectors)

Vec3 values are arrays `[x, y, z]`.

| Function | Description |
|----------|-------------|
| `vec3(x, y, z)` | Create Vec3 |
| `vec3_add(a, b)` | Element-wise add |
| `vec3_sub(a, b)` | Element-wise subtract |
| `vec3_scale(v, s)` | Multiply by scalar |
| `vec3_dot(a, b)` | Dot product |
| `vec3_cross(a, b)` | Cross product |
| `vec3_len(v)` | Magnitude |
| `vec3_normalize(v)` | Unit vector |
| `vec3_dist(a, b)` | Distance |

```akar
let a = vec3(1, 0, 0)
let b = vec3(0, 1, 0)
vec3_cross(a, b)  // [0, 0, 1]
```

## Timing

### `clock()`

Returns CPU time in seconds (for benchmarking).

```akar
let start = clock()
// ... do work ...
let elapsed = clock() - start
print("Time: " + to_string(elapsed) + "s")
```

### `time()`

Returns current Unix timestamp (seconds since epoch, with fractional precision).

```akar
print(time())  // 1716000000.123
```

### `sleep(ms)`

Sleeps for given milliseconds.

```akar
sleep(1000)  // sleep 1 second
```

## I/O

### `input(prompt?)`

Reads a line from stdin. Optional prompt string.

```akar
let name = input("Enter name: ")
print("Hello, " + name)
```

## Program Control

### `exit(code?)`

Exits the program with optional exit code (default 0).

```akar
exit(0)   // success
exit(1)   // error
```

### `assert(condition, message?)`

Terminates with error if condition is falsy.

```akar
assert(x > 0, "x must be positive")
assert(len(arr) > 0)
```

## Random

```akar
random()           // Random float [0, 1)
random() * 100     // Random float [0, 100)
```

## Range

### `range(start, end, step?)`

Creates an array of numbers.

```akar
range(0, 5)       // [0, 1, 2, 3, 4, 5]
range(1, 10, 2)   // [1, 3, 5, 7, 9]
range(5, 0, -1)   // [5, 4, 3, 2, 1, 0]
```

---

## Fibers

Fibers are cooperative coroutines that can yield and resume.

### `fiber_create(fn, ...args)`

Creates a new fiber from a function.

```akar
fn my_gen() {
    fiber_yield(1)
    fiber_yield(2)
    return 3
}
let f = fiber_create(my_gen)
```

### `fiber_resume(fiber, value?)`

Resumes a fiber. Returns the yielded value or return value.

```akar
fiber_resume(f)  // 1 (first yield)
fiber_resume(f)  // 2 (second yield)
fiber_resume(f)  // 3 (return value)
```

### `fiber_yield(value?)`

Yields a value from inside a fiber. The caller receives this value.

```akar
fn counter() {
    let i = 0
    while (true) {
        fiber_yield(i)
        i = i + 1
    }
}
```

### `fiber_status(fiber)`

Returns fiber state: `"created"`, `"running"`, `"suspended"`, or `"done"`.

```akar
let f = fiber_create(fn() { fiber_yield(1) })
fiber_status(f)  // "created"
fiber_resume(f)  // 1
fiber_status(f)  // "suspended"
fiber_resume(f)  // nil (returned)
fiber_status(f)  // "done"
```

### Bidirectional Communication

Values passed to `fiber_resume` become the return value of `fiber_yield`:

```akar
fn accumulator() {
    let total = 0
    while (true) {
        let val = fiber_yield(total)
        if (val == nil) break
        total = total + val
    }
    return total
}

let f = fiber_create(accumulator)
fiber_resume(f)       // 0 (initial yield)
fiber_resume(f, 10)   // 10
fiber_resume(f, 20)   // 30
fiber_resume(f, 5)    // 35
```

### Generator Pattern

```akar
fn range_gen(start, end) {
    let i = start
    while (i <= end) {
        fiber_yield(i)
        i = i + 1
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

collect(fiber_create(range_gen, 1, 5))  // [1, 2, 3, 4, 5]
```

### Nested Fibers

Fibers can create and drive other fibers:

```akar
fn inner() {
    fiber_yield("a")
    fiber_yield("b")
    return "done"
}

fn outer() {
    let f = fiber_create(inner)
    while (true) {
        let v = fiber_resume(f)
        if (fiber_status(f) == "done") return v
        fiber_yield(v)
    }
}

let f = fiber_create(outer)
fiber_resume(f)  // "a"
fiber_resume(f)  // "b"
fiber_resume(f)  // "done"
```
