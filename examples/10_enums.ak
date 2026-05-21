// ============================================================
// Akar Script - Enums
// ============================================================

// Enums define a type with a fixed set of named variants.
// Simple variants are NaN-boxed for instant comparison
// (no heap allocation, no GC pressure).

// --- Basic Enum ---
enum Direction {
    North,
    South,
    East,
    West
}

let dir = Direction.North
print("Direction: " + to_string(dir))
print("Is North: " + to_string(dir == Direction.North))
print("Is South: " + to_string(dir == Direction.South))

// --- Enum in control flow ---
let heading = Direction.East
if (heading == Direction.North) {
    print("Going north!")
} else if (heading == Direction.East) {
    print("Going east!")
} else if (heading == Direction.South) {
    print("Going south!")
} else {
    print("Going west!")
}

// --- Multiple Enum Types ---
enum Color {
    Red,
    Green,
    Blue
}

enum Priority {
    Low,
    Medium,
    High,
    Critical
}

let c = Color.Red
let p = Priority.High
print("Color: " + to_string(c))
print("Priority: " + to_string(p))

// Different enum types are always unequal
print("Same value? " + to_string(c == p))

// --- Enum with Variables ---
enum GameState {
    Menu,
    Playing,
    Paused,
    GameOver
}

let state = GameState.Menu
print("Game state: " + to_string(state))

state = GameState.Playing
print("Now playing!")

state = GameState.Paused
if (state == GameState.Paused) {
    print("Game is paused")
}

print("Enum test done!")
