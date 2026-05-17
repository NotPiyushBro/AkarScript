// ============================================================
// Akar Script - Classes
// Object-oriented programming with classes and methods
// ============================================================

// --- Basic class ---
class Vec2 {
    // Constructor: init() is called when creating an instance
    init(x, y) {
        this.x = x
        this.y = y
    }

    // Instance method (no args besides this)
    length() {
        return sqrt(this.x * this.x + this.y * this.y)
    }

    // Method with argument: returns a new instance
    add(other) {
        return Vec2(this.x + other.x, this.y + other.y)
    }

    // Method with argument: modifies state in place
    scale(factor) {
        this.x = this.x * factor
        this.y = this.y * factor
    }

    // String representation
    to_str() {
        return "Vec2(" + to_string(this.x) + ", " + to_string(this.y) + ")"
    }
}

let a = Vec2(3, 4)
print("a = " + a.to_str())
print("|a| = " + to_string(a.length()))

let b = Vec2(1, 2)
let c = a.add(b)
print("a + b = " + c.to_str())

// Modify in place
a.scale(2)
print("a * 2 = " + a.to_str())

// --- Class with multiple methods ---
class Rectangle {
    init(w, h) {
        this.w = w
        this.h = h
    }
    area() {
        return this.w * this.h
    }
    perimeter() {
        return 2 * (this.w + this.h)
    }
    is_square() {
        return this.w == this.h
    }
    to_str() {
        return to_string(this.w) + "x" + to_string(this.h)
    }
}

let rect = Rectangle(5, 3)
print("\n--- Rectangle ---")
print("Size: " + rect.to_str())
print("Area: " + to_string(rect.area()))
print("Perimeter: " + to_string(rect.perimeter()))
print("Is square: " + to_string(rect.is_square()))

let sq = Rectangle(4, 4)
print("Square: " + sq.to_str() + " is_square=" + to_string(sq.is_square()))

// --- Class with state ---
class Counter {
    init(start) {
        this.value = start
    }
    increment() {
        this.value = this.value + 1
    }
    decrement() {
        this.value = this.value - 1
    }
    get() {
        return this.value
    }
}

print("\n--- Counter ---")
let c2 = Counter(0)
print("start: " + to_string(c2.get()))
c2.increment()
c2.increment()
c2.increment()
print("after 3 increments: " + to_string(c2.get()))
c2.decrement()
print("after 1 decrement: " + to_string(c2.get()))

// --- Composition ---
class Line {
    init(p1, p2) {
        this.p1 = p1
        this.p2 = p2
    }
    length() {
        let dx = this.p2.x - this.p1.x
        let dy = this.p2.y - this.p1.y
        return sqrt(dx * dx + dy * dy)
    }
}

print("\n--- Composition ---")
let line = Line(Vec2(0, 0), Vec2(3, 4))
print("Line length: " + to_string(line.length()))
