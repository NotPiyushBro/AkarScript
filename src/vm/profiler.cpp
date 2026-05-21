#include "akar/vm/profiler.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace akar {

Profiler::Profiler() {
    trace_log_.resize(MAX_TRACE_EVENTS);
    start_time_ = clock::now();
}

void Profiler::start_profiling() {
    if (!profiling_) {
        profiling_ = true;
        start_time_ = clock::now();
    }
}

void Profiler::stop_profiling() {
    profiling_ = false;
}

void Profiler::start_tracing() {
    if (!tracing_) {
        tracing_ = true;
        if (!profiling_) start_profiling(); // tracing implies profiling
    }
}

void Profiler::stop_tracing() {
    tracing_ = false;
}

double Profiler::now_us() const {
    return std::chrono::duration<double, std::micro>(
        clock::now() - start_time_).count();
}

double Profiler::elapsed_us() const {
    return std::chrono::duration<double, std::micro>(
        clock::now() - start_time_).count();
}

// ---- Recording ----

void Profiler::record_call(const char* func_name) {
    if (!func_name) func_name = "<unknown>";
    total_calls_++;

    double t = now_us();

    // Push onto call stack
    call_stack_.push_back({func_name, t, 0.0});

    // Ensure func_stats entry exists
    auto& stats = func_stats_[func_name];
    if (!stats.name) stats.name = func_name;
    stats.call_count++;

    // Trace event
    if (tracing_) {
        TraceEvent ev;
        ev.type = TraceEventType::Call;
        ev.timestamp_us = t;
        ev.name = func_name;
        ev.duration_us = 0;
        ev.extra = 0;
        trace_log_[trace_write_pos_] = ev;
        trace_write_pos_ = (trace_write_pos_ + 1) % MAX_TRACE_EVENTS;
        if (trace_count_ < MAX_TRACE_EVENTS) trace_count_++;
    }
}

void Profiler::record_return(const char* func_name, double duration_us) {
    if (!func_name) func_name = "<unknown>";
    double t = now_us();

    // Pop call stack and compute self-time
    if (!call_stack_.empty()) {
        auto& top = call_stack_.back();
        double inclusive = t - top.enter_time_us;
        double self = inclusive - top.children_time_us;

        auto it = func_stats_.find(top.name);
        if (it != func_stats_.end()) {
            it->second.total_time_us += inclusive;
            it->second.self_time_us += std::max(0.0, self);
        }

        // Add this function's inclusive time to parent's children_time
        if (call_stack_.size() > 1) {
            call_stack_[call_stack_.size() - 2].children_time_us += inclusive;
        }

        call_stack_.pop_back();
    }

    // Trace event
    if (tracing_) {
        TraceEvent ev;
        ev.type = TraceEventType::Return;
        ev.timestamp_us = t;
        ev.name = func_name;
        ev.duration_us = duration_us;
        ev.extra = 0;
        trace_log_[trace_write_pos_] = ev;
        trace_write_pos_ = (trace_write_pos_ + 1) % MAX_TRACE_EVENTS;
        if (trace_count_ < MAX_TRACE_EVENTS) trace_count_++;
    }
}

void Profiler::record_signal_read(const char* signal_name) {
    if (!signal_name) signal_name = "<signal>";
    total_signal_reads_++;

    if (tracing_) {
        TraceEvent ev;
        ev.type = TraceEventType::SignalRead;
        ev.timestamp_us = now_us();
        ev.name = signal_name;
        ev.duration_us = 0;
        ev.extra = 0;
        trace_log_[trace_write_pos_] = ev;
        trace_write_pos_ = (trace_write_pos_ + 1) % MAX_TRACE_EVENTS;
        if (trace_count_ < MAX_TRACE_EVENTS) trace_count_++;
    }
}

void Profiler::record_signal_write(const char* signal_name) {
    if (!signal_name) signal_name = "<signal>";
    total_signal_writes_++;

    if (tracing_) {
        TraceEvent ev;
        ev.type = TraceEventType::SignalWrite;
        ev.timestamp_us = now_us();
        ev.name = signal_name;
        ev.duration_us = 0;
        ev.extra = 0;
        trace_log_[trace_write_pos_] = ev;
        trace_write_pos_ = (trace_write_pos_ + 1) % MAX_TRACE_EVENTS;
        if (trace_count_ < MAX_TRACE_EVENTS) trace_count_++;
    }
}

void Profiler::record_effect_run(const char* effect_name, bool is_rerun) {
    if (!effect_name) effect_name = "<effect>";
    total_effect_runs_++;
    if (is_rerun) total_effect_reruns_++;

    if (tracing_) {
        TraceEvent ev;
        ev.type = is_rerun ? TraceEventType::EffectReRun : TraceEventType::EffectRun;
        ev.timestamp_us = now_us();
        ev.name = effect_name;
        ev.duration_us = 0;
        ev.extra = is_rerun ? 1 : 0;
        trace_log_[trace_write_pos_] = ev;
        trace_write_pos_ = (trace_write_pos_ + 1) % MAX_TRACE_EVENTS;
        if (trace_count_ < MAX_TRACE_EVENTS) trace_count_++;
    }
}

void Profiler::record_gc_start() {
    total_gc_count_++;

    if (tracing_) {
        TraceEvent ev;
        ev.type = TraceEventType::GcStart;
        ev.timestamp_us = now_us();
        ev.name = "GC";
        ev.duration_us = 0;
        ev.extra = 0;
        trace_log_[trace_write_pos_] = ev;
        trace_write_pos_ = (trace_write_pos_ + 1) % MAX_TRACE_EVENTS;
        if (trace_count_ < MAX_TRACE_EVENTS) trace_count_++;
    }
}

void Profiler::record_gc_end(double duration_us, size_t bytes_freed) {
    total_gc_time_us_ += duration_us;
    total_gc_bytes_freed_ += bytes_freed;

    if (tracing_) {
        TraceEvent ev;
        ev.type = TraceEventType::GcEnd;
        ev.timestamp_us = now_us();
        ev.name = "GC";
        ev.duration_us = duration_us;
        ev.extra = bytes_freed;
        trace_log_[trace_write_pos_] = ev;
        trace_write_pos_ = (trace_write_pos_ + 1) % MAX_TRACE_EVENTS;
        if (trace_count_ < MAX_TRACE_EVENTS) trace_count_++;
    }
}

void Profiler::record_opcodes(uint64_t count) {
    total_opcodes_ += count;
    // Attribute to current function on the call stack
    if (!call_stack_.empty()) {
        auto it = func_stats_.find(call_stack_.back().name);
        if (it != func_stats_.end()) {
            it->second.opcode_count += count;
        }
    }
}

// ---- Reporting ----

std::vector<FuncStats> Profiler::get_function_stats() const {
    std::vector<FuncStats> result;
    result.reserve(func_stats_.size());
    for (auto& [name, stats] : func_stats_) {
        result.push_back(stats);
    }
    // Sort by self-time descending
    std::sort(result.begin(), result.end(), [](const FuncStats& a, const FuncStats& b) {
        return a.self_time_us > b.self_time_us;
    });
    return result;
}

static const char* trace_event_name(TraceEventType t) {
    switch (t) {
        case TraceEventType::Call:         return "CALL";
        case TraceEventType::Return:       return "RET ";
        case TraceEventType::SignalRead:   return "SIGR";
        case TraceEventType::SignalWrite:  return "SIGW";
        case TraceEventType::EffectRun:    return "EFF ";
        case TraceEventType::EffectReRun:  return "EFF!";
        case TraceEventType::GcStart:      return "GC  ";
        case TraceEventType::GcEnd:        return "GCE ";
        case TraceEventType::Opcode:       return "OP  ";
    }
    return "????";
}

void Profiler::print_profile_report() const {
    double total_time = elapsed_us();
    fprintf(stderr, "\n");
    fprintf(stderr, "╔══════════════════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║                  AKAR SCRIPT PROFILE REPORT                 ║\n");
    fprintf(stderr, "╠══════════════════════════════════════════════════════════════╣\n");

    // Summary
    fprintf(stderr, "║  Total time:        %10.2f ms                          ║\n", total_time / 1000.0);
    fprintf(stderr, "║  Total opcodes:     %10llu                              ║\n",
        (unsigned long long)total_opcodes_);
    fprintf(stderr, "║  Total calls:       %10llu                              ║\n",
        (unsigned long long)total_calls_);
    fprintf(stderr, "║  Signal writes:     %10llu                              ║\n",
        (unsigned long long)total_signal_writes_);
    fprintf(stderr, "║  Effect runs:       %10llu  (re-runs: %llu)              ║\n",
        (unsigned long long)total_effect_runs_,
        (unsigned long long)total_effect_reruns_);
    fprintf(stderr, "║  GC cycles:         %10llu                              ║\n",
        (unsigned long long)total_gc_count_);

    if (total_gc_count_ > 0) {
        fprintf(stderr, "║  GC time:           %10.2f ms  (%.1f%% of total)        ║\n",
            total_gc_time_us_ / 1000.0,
            total_time > 0 ? (total_gc_time_us_ / total_time * 100.0) : 0.0);
        fprintf(stderr, "║  GC bytes freed:    %10llu bytes                       ║\n",
            (unsigned long long)total_gc_bytes_freed_);
    }

    double mops = total_time > 0 ? (total_opcodes_ / total_time) : 0;
    fprintf(stderr, "║  Throughput:        %10.2f M ops/sec                    ║\n", mops);
    fprintf(stderr, "╠══════════════════════════════════════════════════════════════╣\n");

    // Function table
    auto stats = get_function_stats();
    if (!stats.empty()) {
        fprintf(stderr, "║  FUNCTION PROFILE (sorted by self-time)                    ║\n");
        fprintf(stderr, "╠══════════════════════════════════════════════════════════════╣\n");
        fprintf(stderr, "║  %-20s %6s %10s %10s %8s  ║\n",
            "Function", "Calls", "Total(ms)", "Self(ms)", "Opcodes");
        fprintf(stderr, "║  %-20s %6s %10s %10s %8s  ║\n",
            "────────────────────", "──────", "──────────", "──────────", "────────");

        int shown = 0;
        for (auto& s : stats) {
            if (shown >= 20) {
                fprintf(stderr, "║  ... (%zu more functions)                              ║\n",
                    stats.size() - 20);
                break;
            }
            const char* name = s.name ? s.name : "<unknown>";
            // Truncate long names
            char short_name[21];
            snprintf(short_name, sizeof(short_name), "%s", name);

            fprintf(stderr, "║  %-20s %6llu %10.2f %10.2f %8llu  ║\n",
                short_name,
                (unsigned long long)s.call_count,
                s.total_time_us / 1000.0,
                s.self_time_us / 1000.0,
                (unsigned long long)s.opcode_count);
            shown++;
        }
    }

    fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n");
    fprintf(stderr, "\n");
}

void Profiler::print_trace_log() const {
    fprintf(stderr, "\n╔══════════════════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║                    AKAR SCRIPT TRACE LOG                    ║\n");
    fprintf(stderr, "╠══════════════════════════════════════════════════════════════╣\n");
    fprintf(stderr, "║  %-8s %12s  %-20s %10s  ║\n",
        "Type", "Time(ms)", "Name", "Duration");
    fprintf(stderr, "║  %-8s %12s  %-20s %10s  ║\n",
        "────", "────────", "────", "────────");

    size_t count = std::min(trace_count_, MAX_TRACE_EVENTS);
    size_t start = (trace_count_ >= MAX_TRACE_EVENTS) ? trace_write_pos_ : 0;

    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % MAX_TRACE_EVENTS;
        auto& ev = trace_log_[idx];

        const char* name = ev.name ? ev.name : "";
        char short_name[21];
        snprintf(short_name, sizeof(short_name), "%s", name);

        if (ev.type == TraceEventType::Return || ev.type == TraceEventType::GcEnd) {
            fprintf(stderr, "║  %-8s %12.3f  %-20s %10.3f  ║\n",
                trace_event_name(ev.type),
                ev.timestamp_us / 1000.0,
                short_name,
                ev.duration_us / 1000.0);
        } else {
            fprintf(stderr, "║  %-8s %12.3f  %-20s %10s  ║\n",
                trace_event_name(ev.type),
                ev.timestamp_us / 1000.0,
                short_name,
                "");
        }
    }

    if (count == 0) {
        fprintf(stderr, "║  (no trace events recorded)                                 ║\n");
    }

    fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n");
    fprintf(stderr, "\n");
}

void Profiler::reset() {
    func_stats_.clear();
    call_stack_.clear();
    trace_write_pos_ = 0;
    trace_count_ = 0;
    total_opcodes_ = 0;
    total_calls_ = 0;
    total_signal_reads_ = 0;
    total_signal_writes_ = 0;
    total_effect_runs_ = 0;
    total_effect_reruns_ = 0;
    total_gc_count_ = 0;
    total_gc_time_us_ = 0.0;
    total_gc_bytes_freed_ = 0;
    start_time_ = clock::now();
}

} // namespace akar
