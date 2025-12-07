#include "scheduler_utils.h"
#include "gpio.h"
#include <stdio.h>

// Compute difference in microseconds.
// Used for jitter and deadline checks.
long diff_us(struct timespec a, struct timespec b) {
    long sec  = a.tv_sec - b.tv_sec;
    long nsec = a.tv_nsec - b.tv_nsec;
    return sec * 1000000L + nsec / 1000L;
}

void add_ms(struct timespec* t, long ms) {
    t->tv_sec  += ms / 1000;
    t->tv_nsec += (ms % 1000) * 1000000L;
    if (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        t->tv_sec  += 1;
    }
}

// Simulates the WCET of a task.
// Busy-waiting keeps timing comparable across schedulers.
void busy_compute(long ms) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    long target = ms * 1000L;
    do {
        clock_gettime(CLOCK_MONOTONIC, &now);
    } while (diff_us(now, start) < target);
}

// Turns alarm LED on whenever a deadline miss occurs.
// Does not turn it off — creates a "persistent fault" indicator.
void deadline_latch_alarm(int alarm_gpio, int misses) {
    if (misses > 0)
        set_gpio_value(alarm_gpio, 1);
}

void init_gpio_pins(int g1, int g2, int g3, int alarm_gpio) {
    int pins[4] = {g1, g2, g3, alarm_gpio};
    for (int i = 0; i < 4; i++) {
        export_gpio(pins[i]);
        set_gpio_direction(pins[i], "out");
        set_gpio_value(pins[i], 0);
    }
}

void reset_gpio_pins(int g1, int g2, int g3, int alarm_gpio) {
    set_gpio_value(g1, 0);
    set_gpio_value(g2, 0);
    set_gpio_value(g3, 0);
    set_gpio_value(alarm_gpio, 0);
}

// Shared reporting block.
//
// All schedulers produce identical-form statistics so comparisons
// are consistent and presentation-friendly.
void print_scheduler_report(const char* title,
                            TaskStats* stats,
                            int count,
                            double runtime_sec)
{
    printf("=== %s ===\n\n", title);

    int total_misses = 0;

    for (int i = 0; i < count; i++) {
        double avg_ms = (stats[i].jobs > 0)
            ? (double)stats[i].total_jitter_us / stats[i].jobs / 1000.0
            : 0;

        printf("%s:\n", stats[i].name);
        printf("  Worst jitter: %.3f ms\n",
               stats[i].worst_jitter_us / 1000.0);
        printf("  Avg jitter:   %.3f ms\n", avg_ms);
        printf("  Deadline misses: %d\n\n",
               stats[i].deadline_misses);

        total_misses += stats[i].deadline_misses;
    }

    printf("TOTAL deadline misses: %d\n", total_misses);
    printf("Runtime: %.3f sec\n\n", runtime_sec);
}
