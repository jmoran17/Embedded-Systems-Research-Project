#ifndef SCHEDULER_UTILS_H
#define SCHEDULER_UTILS_H

#include <time.h>

// ----- Timing Helpers -----
// Used by all schedulers to measure jitter, deadlines, and simulate workload.

// Returns (a - b) in microseconds
long diff_us(struct timespec a, struct timespec b);

// Add ms to a timespec safely (handles nanosecond rollover)
void add_ms(struct timespec* t, long ms);

// Busy-wait loop used to simulate WCET.
// Gives consistent behavior across schedulers for fair comparison.
void busy_compute(long ms);

// ----- GPIO Helpers -----

// Latches the alarm LED ON as soon as any deadline miss occurs.
// Never turned off automatically; presenter can point to LED as a visual indicator.
void deadline_latch_alarm(int alarm_gpio, int misses);

// Initialize three task LEDs + alarm LED, all set low initially.
void init_gpio_pins(int g1, int g2, int g3, int alarm_gpio);

// Clear all LEDs after scheduler completes.
void reset_gpio_pins(int g1, int g2, int g3, int alarm_gpio);

// ----- Statistic Struct -----

// Shared representation of per-task statistics.
// All three schedulers convert their internal structs into this format.
struct TaskStats {
    const char* name;
    long worst_jitter_us;
    long total_jitter_us;
    int jobs;
    int deadline_misses;
};

// Unified report generator (presentation-friendly output).
void print_scheduler_report(const char* title,
                            TaskStats* stats,
                            int count,
                            double runtime_sec);

#endif
