// ============================================================
// Akar Script - Signals & Effects (Reactive Primitives)
// ============================================================

// Signals are reactive values. When a signal changes, any
// effect that depends on it automatically re-runs.
// This is ZERO-BOILERPLATE reactivity - no manual event wiring.

// --- Basic Signal ---
signal counter = 0
print("Initial counter: " + to_string(counter))

counter = 1
print("After set: " + to_string(counter))

counter = 42
print("After second set: " + to_string(counter))

// --- Effect with Signal ---
// An effect runs once immediately (to track dependencies),
// then re-runs automatically whenever any signal it reads changes.
signal temperature = 20

effect {
    if (temperature > 30) {
        print("HOT! Temperature is " + to_string(temperature))
    } else if (temperature < 10) {
        print("COLD! Temperature is " + to_string(temperature))
    } else {
        print("Nice temperature: " + to_string(temperature))
    }
}

print("--- Changing temperature ---")
temperature = 35
temperature = 5
temperature = 22

// --- Multiple Signals in Effect ---
// An effect tracks ALL signals it reads, and re-runs
// when ANY of them change.
signal x = 10
signal y = 20

effect {
    print("Sum: " + to_string(x + y))
}

print("--- Changing x and y ---")
x = 100
y = 200

print("Done!")
