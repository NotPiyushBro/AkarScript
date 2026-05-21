#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace akar {

// ============================================================
// Trace Event — one recorded event in the trace log
// ============================================================

enum class TraceEventType : uint8_t {
    Call,           // function entered
    Return,         // function returned
    SignalRead,     // signal value read (inside effect)
    SignalWrite,    // signal value written
    EffectRun,      // effect body executed
    EffectReRun,    // effect re-executed due to signal change
    GcStart,        // GC cycle started
    GcEnd,          // GC cycle ended
    Opcode,         // opcode executed (only when opcode-level tracing is on)
};

struct TraceEvent {
    TraceEventType type;
    double timestamp_us;        // microseconds since profiling started
    const char* name;           // function/signal/effect name (interned string)
    double duration_us;         // for Return/GcEnd: how long it took
    uint64_t extra;             // opcode count, GC bytes freed, etc.
};

// ============================================================
// Function Profile Stats
// ============================================================

struct FuncStats {
    const char* name = nullptr;
    uint64_t call_count = 0;
    double total_time_us = 0.0;     // inclusive time (including callees)
    double self_time_us = 0.0;      // exclusive time (excluding callees)
    uint64_t opcode_count = 0;      // opcodes executed inside this function
};

// ============================================================
// Profiler — aggregates stats, records trace events
// ============================================================

class Profiler {
public:
    using clock = std::chrono::steady_clock;

    Profiler();

    // Control
    void start_profiling();
    void stop_profiling();
    void start_tracing();
    void stop_tracing();
    bool is_profiling() const { return profiling_; }
    bool is_tracing() const { return tracing_; }

    // Record events (called from VM)
    void record_call(const char* func_name);
    void record_return(const char* func_name, double duration_us);
    void record_signal_read(const char* signal_name);
    void record_signal_write(const char* signal_name);
    void record_effect_run(const char* effect_name, bool is_rerun);
    void record_gc_start();
    void record_gc_end(double duration_us, size_t bytes_freed);
    void record_opcodes(uint64_t count);

    // Query
    double elapsed_us() const;
    uint64_t total_opcodes() const { return total_opcodes_; }
    uint64_t total_calls() const { return total_calls_; }
    uint64_t total_signal_writes() const { return total_signal_writes_; }
    uint64_t total_effect_runs() const { return total_effect_runs_; }
    uint64_t total_gc_count() const { return total_gc_count_; }
    double total_gc_time_us() const { return total_gc_time_us_; }

    // Report
    void print_profile_report() const;
    void print_trace_log() const;

    // Get all function stats sorted by self-time
    std::vector<FuncStats> get_function_stats() const;

    // Reset
    void reset();

    // Timestamp
    double now_us() const;

private:
    bool profiling_ = false;
    bool tracing_ = false;
    clock::time_point start_time_;

    // Per-function stats
    std::unordered_map<const char*, FuncStats> func_stats_;

    // Call stack for computing self-time
    struct CallEntry {
        const char* name;
        double enter_time_us;
        double children_time_us;  // time spent in callees
    };
    std::vector<CallEntry> call_stack_;

    // Trace log (ring buffer)
    static constexpr size_t MAX_TRACE_EVENTS = 64 * 1024;  // 64K events
    std::vector<TraceEvent> trace_log_;
    size_t trace_write_pos_ = 0;
    size_t trace_count_ = 0;

    // Aggregate counters
    uint64_t total_opcodes_ = 0;
    uint64_t total_calls_ = 0;
    uint64_t total_signal_reads_ = 0;
    uint64_t total_signal_writes_ = 0;
    uint64_t total_effect_runs_ = 0;
    uint64_t total_effect_reruns_ = 0;
    uint64_t total_gc_count_ = 0;
    double total_gc_time_us_ = 0.0;
    size_t total_gc_bytes_freed_ = 0;
};

} // namespace akar
