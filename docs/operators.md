# Operator Overloading

Akar supports operator overloading through magic methods (double-underscore methods) on classes.

## Available Operators

| Operator | Magic Method | Signature | Example |
|----------|-------------|-----------|---------|
| `a + b` | `__add` | `__add(self, other)` | `Vec2(1,2) + Vec2(3,4)` |
| `a - b` | `__sub` | `__sub(self, other)` | `Vec2(1,2) - Vec2(3,4)` |
| `a * b` | `__mul` | `__mul(self, other)` | `Vec2(1,2) * 2` |
| `a / b` | `__div` | `__div(self, other)` | `a / b` |
| `a % b` | `__mod` | `__mod(self, other)` | `a % b` |
| `-a` | `__neg` | `__neg(self)` | `-Vec2(1,2)` |
| `a == b` | `__eq` | `__eq(self, other)` | `a == b` |
| `a < b` | `__lt` | `__lt(self, other)` | `a < b` |
| `a <= b` | `__le` | `__le(self, other)` | `a <= b` |
| `a > b` | `__gt` | `__gt(self, other)` | `a > b` |
| `a >= b` | `__ge` | `__ge(self, other)` | `a >= b` |

## How It Works

1. When an operator like `a + b` is executed, the VM first checks if both operands are compatible types (numbers + numbers, strings + strings)
2. If types are incompatible, the VM looks for `__add` on the **left operand's** class
3. If found, it calls `a.__add(b)` and uses the return value
4. If not found, a runtime error is thrown

**Fast path preserved:** When both operands are numbers, the VM adds them directly without any method lookup. Operator overloading only activates on type mismatch.

## Examples

### Vec2 (2D Vector)

```akar
class Vec2 {
    init(x, y) {
        this.x = x
        this.y = y
    }

    __add(o) { return Vec2(this.x + o.x, this.y + o.y) }
    __sub(o) { return Vec2(this.x - o.x, this.y - o.y) }
    __mul(s) { return Vec2(this.x * s, this.y * s) }
    __div(s) { return Vec2(this.x / s, this.y / s) }
    __neg()  { return Vec2(-this.x, -this.y) }
    __eq(o)  { return this.x == o.x and this.y == o.y }
    __lt(o)  {
        return (this.x * this.x + this.y * this.y) <
               (o.x * o.x + o.y * o.y)
    }

    len()    { return sqrt(this.x * this.x + this.y * this.y) }
    normalize() {
        let l = this.len()
        return Vec2(this.x / l, this.y / l)
    }
    dot(o)   { return this.x * o.x + this.y * o.y }

    str() { return "Vec2(" + to_string(this.x) + "," + to_string(this.y) + ")" }
}

// Usage
let a = Vec2(3, 4)
let b = Vec2(1, 2)

a + b          // Vec2(4, 6)
a - b          // Vec2(2, 2)
a * 2          // Vec2(6, 8)
a / 2          // Vec2(1.5, 2)
-a             // Vec2(-3, -4)
a == b         // false
a == Vec2(3,4) // true

// Chaining
let c = a * 0.5 + b  // Vec2(2.5, 4)

// Methods still work
a.len()        // 5
a.dot(b)       // 11
a.normalize()  // Vec2(0.6, 0.8)
```

### Color

```akar
class Color {
    init(r, g, b) {
        this.r = clamp(r, 0, 255)
        this.g = clamp(g, 0, 255)
        this.b = clamp(b, 0, 255)
    }

    __add(o) { return Color(this.r + o.r, this.g + o.g, this.b + o.b) }
    __sub(o) { return Color(this.r - o.r, this.g - o.g, this.b - o.b) }
    __mul(s) { return Color(this.r * s, this.g * s, this.b * s) }
    __eq(o)  { return this.r == o.r and this.g == o.g and this.b == o.b }

    lerp(o, t) {
        return Color(
            this.r + (o.r - this.r) * t,
            this.g + (o.g - this.g) * t,
            this.b + (o.b - this.b) * t
        )
    }

    str() {
        return "Color(" + to_string(this.r) + "," +
               to_string(this.g) + "," + to_string(this.b) + ")"
    }
}

let red   = Color(255, 0, 0)
let green = Color(0, 255, 0)
let blue  = Color(0, 0, 255)

red + green         // Color(255, 255, 0) = yellow
red * 0.5           // Color(127, 0, 0) = dark red
red.lerp(green, 0.5) // Color(127, 127, 0) = olive

// Chained blending
let sky = blue * 0.7 + Color(135, 206, 235) * 0.3
```

### Matrix2x2

```akar
class Mat2 {
    init(a, b, c, d) {
        this.a = a  this.b = b
        this.c = c  this.d = d
    }

    __mul(other) {
        if (type(other) == "number") {
            return Mat2(this.a * other, this.b * other,
                       this.c * other, this.d * other)
        }
        // Matrix multiplication
        return Mat2(
            this.a * other.a + this.b * other.c,
            this.a * other.b + this.b * other.d,
            this.c * other.a + this.d * other.c,
            this.c * other.b + this.d * other.d
        )
    }

    det() { return this.a * this.d - this.b * this.c }

    str() {
        return "[[" + to_string(this.a) + "," + to_string(this.b) + "][" +
               to_string(this.c) + "," + to_string(this.d) + "]]"
    }
}

let m = Mat2(1, 2, 3, 4)
m * 2      // scalar multiply
m * m      // matrix multiply
m.det()    // -2
```

### Physics Body

```akar
class Vec2 {
    init(x, y) { this.x = x  this.y = y }
    __add(o) { return Vec2(this.x + o.x, this.y + o.y) }
    __sub(o) { return Vec2(this.x - o.x, this.y - o.y) }
    __mul(s) { return Vec2(this.x * s, this.y * s) }
    len() { return sqrt(this.x * this.x + this.y * this.y) }
    str() { return "(" + to_string(this.x) + "," + to_string(this.y) + ")" }
}

class Body {
    init(x, y, mass) {
        this.pos = Vec2(x, y)
        this.vel = Vec2(0, 0)
        this.force = Vec2(0, 0)
        this.mass = mass
    }

    apply_force(f) {
        this.force = this.force + f
    }

    update(dt) {
        let accel = this.force * (1.0 / this.mass)
        this.vel = this.vel + accel * dt
        this.pos = this.pos + this.vel * dt
        this.force = Vec2(0, 0)
    }
}

let ball = Body(0, 0, 1.0)
ball.apply_force(Vec2(100, 0))
ball.update(0.016)  // ~60fps
print(ball.pos.str())  // (1.6, 0)
```

## Rules

1. **Left operand is `this`**: In `a + b`, `a.__add(b)` is called. `this` = `a`, parameter = `b`.

2. **Right operand can be any type**: `Vec2 * 2` works because `__mul` receives `2` as its parameter. Your code decides what to do.

3. **No method = error**: If neither operand has the magic method, a runtime error occurs.

4. **Return value replaces the expression**: `a + b` evaluates to whatever `__add` returns.

5. **`__eq` overrides pointer comparison**: Without `__eq`, instances are compared by identity (pointer). With `__eq`, you control equality semantics.

6. **Performance**: The fast path (number + number, string + string) is unchanged. Operator overloading only activates when types don't match the fast path.

7. **Unary `__neg`**: Takes no parameters besides `self`. Called when `-a` is used and `a` is not a number.
