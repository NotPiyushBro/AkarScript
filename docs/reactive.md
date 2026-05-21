# Reactive Signals & Effects

Akar Script has **built-in reactive state management** as a language primitive. No other scripting language has this — no Lua, no Python, no JavaScript. Frameworks like Vue.js and Solid.js have it, but those are UI frameworks. Akar brings reactivity to general-purpose game scripting at the VM level.

## Overview

**Signals** are reactive variables. When a signal's value changes, any **effect** that read it automatically re-runs.

```akar
signal health = 100

effect {
    print("Health: " + to_string(health))
}
// Prints "Health: 100" immediately

health = 80
// Prints "Health: 80" automatically — no callbacks, no events
```

## Signals

### Declaration

```akar
signal name = initial_value
```

A signal is a reactive variable. It can hold any Akar value (number, string, array, map, instance, etc.).

```akar
signal hp = 100
signal name = "Hero"
signal inventory = ["sword", "shield"]
signal config = {"volume": 0.8, "brightness": 1.0}
```

### Reading

Read a signal like any variable:

```akar
signal x = 10
print(x)        // 10
let y = x + 5   // y = 15
```

When read **inside an effect**, the VM automatically tracks the dependency. When read **outside an effect**, there is zero overhead — it's just a pointer dereference.

### Writing

Write to a signal with `=`:

```akar
signal x = 10
x = 20      // triggers dependent effects
x = x + 1   // triggers again
```

## Effects

### Declaration

```akar
effect {
    // body — reads signals, does side effects
}
```

An effect block:
1. **Runs once immediately** when declared (to discover which signals it reads)
2. **Re-runs automatically** whenever any signal it read last time changes
3. **Rebuilds its dependency list** on each run (dynamic dependency tracking)

### Examples

#### Auto-updating HUD

```akar
signal player_hp = 100
signal player_mana = 50

effect {
    update_health_bar(player_hp)
    update_mana_bar(player_mana)
}
```

Any code that writes to `player_hp` or `player_mana` automatically triggers the effect.

#### Conditional dependencies

```akar
signal mode = "easy"
signal score = 0

effect {
    if (mode == "hard") {
        // Only reads `score` when mode is "hard"
        print("Score: " + to_string(score))
    } else {
        print("Easy mode")
    }
}
```

When `mode` is `"easy"`, changing `score` does NOT re-run the effect (it wasn't read). When `mode` is `"hard"`, changing `score` DOES re-run it. The dependency list is rebuilt on every execution.

#### Game state machine

```akar
enum GameState { Menu, Playing, Paused, GameOver }
signal state = GameState.Menu

effect {
    if (state == GameState.Menu) {
        show_menu()
    } else if (state == GameState.Playing) {
        hide_menu()
        resume_game()
    } else if (state == GameState.GameOver) {
        show_death_screen()
    }
}
```

#### Multiple signals, one effect

```akar
signal x = 10
signal y = 20

effect {
    print("Sum = " + to_string(x + y))
}

x = 100   // "Sum = 120" (x changed, effect re-runs, reads both)
y = 200   // "Sum = 300" (y changed, effect re-runs)
```

## How Dependency Tracking Works

The VM uses a `current_effect_` pointer:

```
Normal code:     current_effect_ = null
                 → SIGNAL_GET just returns the value
                 → Zero overhead

Inside effect:   current_effect_ = <effect object>
                 → SIGNAL_GET also registers the dependency
                 → One pointer check (CPU branch predictor optimizes this)
```

When a signal is written:
1. Update the signal's value
2. Iterate the signal's subscriber list (SmallVec, typically 1-3 entries)
3. Queue each subscriber effect for re-execution
4. Effects are batched — multiple writes before drain only trigger 1 re-run per effect

When an effect re-runs:
1. Remove itself from all old signal subscriber lists (O(1) swap-remove)
2. Clear its dependency list
3. Execute the body (new dependencies are tracked automatically)

## Batching

Multiple signal writes before the next dispatch cycle are batched:

```akar
signal a = 0
signal b = 0

effect {
    print("a=" + to_string(a) + " b=" + to_string(b))
}

a = 1
b = 2
a = 3
// Only ONE effect re-run with a=3, b=2 — not three separate runs
```

## Performance Characteristics

| Operation | Cost | Notes |
|-----------|------|-------|
| Signal read (no effect) | 1 pointer deref | Same as normal variable |
| Signal read (in effect) | 1 pointer deref + 1 branch | Dependency tracked |
| Signal write | O(subscribers) | SmallVec, typically 1-3 |
| Effect re-run | O(old_deps + body + new_deps) | Swap-remove cleanup |
| Heap allocation per signal | 0 | SmallVec inline buffer |
| Heap allocation per effect | 0 | SmallVec inline buffer |

For typical game use (5-20 signals, 1-5 effects), the overhead is ~1μs per signal write + effect re-run.

## Native API

These functions are available from Akar script:

```akar
signal_value(sig)    // Read signal value without dependency tracking
```

## CLI Flags

```bash
akar --profile file.ak   # See signal write and effect run counts in the profile report
akar --trace file.ak     // See every signal read/write and effect run in the trace log
```

## See Also

- [Enums](language.md#enums) — often used with signals for state machines
- [Profiling & Tracing](profiling.md) — measure signal/effect overhead
- [Fibers](stdlib.md#fibers) — another concurrency primitive
