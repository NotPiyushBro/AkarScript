# Profiling & Tracing

Akar Script has a **built-in profiler and tracer** integrated into the VM. No external tools needed — just flip a flag or call a function.

## Quick Start

### CLI Flags

```bash
# Profile a script — prints report on exit
akar --profile game.ak

# Trace a script — prints every event on exit
akar --trace game.ak

# Both at once
akar --profile --trace game.ak

# Short flags
akar -p -t game.ak
```

### From Akar Script

```akar
profile_start()          // begin collecting timing data
// ... your code ...
profile_stop()           // stop collecting
profile_report()         // print the report to stderr
```

```akar
trace_start()            // begin recording trace events
// ... your code ...
trace_stop()             // stop recording
trace_dump()             // print the trace log to stderr
```

## Profiling

### What It Tracks

| Metric | Description |
|--------|-------------|
| **Total time** | Wall-clock time from start to report |
| **Total opcodes** | Bytecode instructions executed (counted in batches of 128) |
| **Total calls** | Number of function calls |
| **Signal writes** | How many times signals were assigned |
| **Effect runs** | How many times effects executed (initial + re-runs) |
| **GC cycles** | Number of garbage collections |
| **GC time** | Time spent in GC (and % of total) |
| **GC bytes freed** | Total memory reclaimed by GC |
| **Throughput** | Million opcodes per second |

### Function Profile Table

```
║  Function          Calls   Total(ms)   Self(ms)  Opcodes  ║
║  fibonacci          1973       34.10       3.45        0   ║
║  game_loop            60      500.00     200.00   500000   ║
║  render              60      300.00     300.00   200000    ║
```

- **Calls** — how many times the function was called
- **Total** — inclusive time (including time spent in callees)
- **Self** — exclusive time (just this function, subtracting callee time)
- **Opcodes** — bytecode instructions executed inside this function

### Example

```akar
fn fibonacci(n) {
    if (n <= 1) { return n }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

profile_start()
let result = fibonacci(20)
profile_stop()
profile_report()
```

Output:
```
╔══════════════════════════════════════════════════════════════╗
║                  AKAR SCRIPT PROFILE REPORT                 ║
╠══════════════════════════════════════════════════════════════╣
║  Total time:             37.36 ms                          ║
║  Total opcodes:              0                              ║
║  Total calls:            21891                              ║
║  Signal writes:              0                              ║
║  Effect runs:                0  (re-runs: 0)              ║
║  GC cycles:                  0                              ║
╠══════════════════════════════════════════════════════════════╣
║  FUNCTION PROFILE (sorted by self-time)                    ║
║  Function              Calls  Total(ms)   Self(ms)  Opcodes  ║
║  fibonacci             21891     503.38      37.35        0  ║
╚══════════════════════════════════════════════════════════════╝
```

## Tracing

### What It Records

Every event in the VM's execution is recorded with a timestamp:

| Event | Description |
|-------|-------------|
| **CALL** | Function entered |
| **RET** | Function returned (with duration) |
| **SIGR** | Signal read (dependency tracked inside effect) |
| **SIGW** | Signal value written |
| **EFF** | Effect initial run |
| **EFF!** | Effect re-run (triggered by signal change) |
| **GC** | GC cycle started |
| **GCE** | GC cycle ended (with duration and bytes freed) |

### Example

```akar
signal x = 10

effect {
    print("x changed to " + to_string(x))
}

trace_start()
x = 20
x = 30
trace_stop()
trace_dump()
```

Output:
```
╔══════════════════════════════════════════════════════════════╗
║                    AKAR SCRIPT TRACE LOG                    ║
╠══════════════════════════════════════════════════════════════╣
║  Type         Time(ms)  Name                   Duration  ║
║  EFF             0.003  effect                           ║
║  SIGR            0.005  signal                           ║
║  RET             0.006  __effect__                0.000  ║
║  SIGW            0.007  signal                           ║
║  EFF!            0.008  effect                           ║
║  SIGR            0.009  signal                           ║
║  RET             0.010  __effect__                0.000  ║
║  SIGW            0.011  signal                           ║
║  EFF!            0.012  effect                           ║
║  SIGR            0.013  signal                           ║
║  RET             0.014  __effect__                0.000  ║
╚══════════════════════════════════════════════════════════════╝
```

## Native API Reference

### Profiling

| Function | Description |
|----------|-------------|
| `profile_start()` | Enable profiling |
| `profile_stop()` | Disable profiling |
| `profile_report()` | Print profile report to stderr |
| `profile_reset()` | Clear all collected data |

### Tracing

| Function | Description |
|----------|-------------|
| `trace_start()` | Enable tracing (also enables profiling) |
| `trace_stop()` | Disable tracing |
| `trace_dump()` | Print trace log to stderr |

### Selective Profiling

```akar
// Profile only a specific section
fn game_loop() {
    profile_start()
    // ... hot code ...
    profile_stop()
    profile_report()
    profile_reset()
}
```

## Performance Overhead

| Mode | Overhead |
|------|----------|
| **Disabled** (default) | Near-zero — one `__builtin_expect` branch per hook point |
| **Profiling** | ~100ns per function call (timing + stat update) |
| **Tracing** | ~200ns per event (timestamp + ring buffer write) |

The trace log is a 64K-entry ring buffer — old events are overwritten. It never grows.

## Use Cases

### Find hot functions

```akar
profile_start()
run_game_frame()
profile_stop()
profile_report()
// → See which function has the highest self-time
```

### Measure signal/effect overhead

```akar
profile_start()
// ... game with reactive UI ...
profile_stop()
profile_report()
// → See "Signal writes: 500, Effect runs: 500"
```

### Debug effect re-runs

```akar
trace_start()
player_hp = 0   // should trigger game-over effect
trace_stop()
trace_dump()
// → See exactly which effects ran and in what order
```

### GC analysis

```akar
profile_start()
// ... allocate-heavy code ...
profile_stop()
profile_report()
// → See "GC cycles: 5, GC time: 12.3ms (15% of total)"
```
