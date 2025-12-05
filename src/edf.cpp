#include "edf.h"
#include "gpio.h"
#include "scheduler_utils.h"

#include <time.h>
#include <stdio.h>

// EDF task definition.
// Tracks deadlines and next release times.
struct EDFTask {
    const char* name;
    int gpio;
    long period_ms;
    long compute_ms;

    struct timespec next_release;   // next time this job becomes ready
    struct timespec deadline;       // absolute deadline for current job

    long worst_jitter_us;
    long total_jitter_us;
    int jobs;
    int deadline_misses;
};

int run_edf(int jobs_per_task) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int g1 = 17, g2 = 27, g3 = 22, alarm = 5;
    init_gpio_pins(g1, g2, g3, alarm);

    // EDF task set: small differences in WCET and period
    EDFTask tasks[3] = {
        {"T1_10ms",  g1, 10, 1},
        {"T2_50ms",  g2, 50, 2},
        {"T3_100ms", g3, 100, 3}
    };

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // Initialize all EDF timing elements
    for (int i = 0; i < 3; i++) {
        tasks[i].next_release = now;
        tasks[i].deadline     = now;
        add_ms(&tasks[i].deadline, tasks[i].period_ms);
        tasks[i].worst_jitter_us  = 0;
        tasks[i].total_jitter_us  = 0;
        tasks[i].jobs             = 0;
        tasks[i].deadline_misses  = 0;
    }

    int remaining = jobs_per_task * 3;

    // Main EDF superloop
    while (remaining > 0) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        int best = -1;
        struct timespec best_dl;

        // Find the ready task with the earliest deadline
        for (int i = 0; i < 3; i++) {
            if (tasks[i].jobs >= jobs_per_task)
                continue;

            // Not ready yet
            if (diff_us(now, tasks[i].next_release) < 0)
                continue;

            // If first ready or earlier deadline, select it
            if (best == -1 ||
                diff_us(tasks[i].deadline, best_dl) < 0) {
                best = i;
                best_dl = tasks[i].deadline;
            }
        }

        // If no task is ready, jump to earliest next release
        if (best == -1) {
            struct timespec earliest = tasks[0].next_release;
            for (int i = 1; i < 3; i++) {
                if (tasks[i].jobs < jobs_per_task &&
                    diff_us(tasks[i].next_release, earliest) < 0)
                    earliest = tasks[i].next_release;
            }
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &earliest, NULL);
            continue;
        }

        // Run the selected task (non-preemptive execution)
        EDFTask* t = &tasks[best];

        struct timespec exec_start;
        clock_gettime(CLOCK_MONOTONIC, &exec_start);

        long jitter = diff_us(exec_start, t->next_release);
        if (jitter < 0) jitter = -jitter;
        t->total_jitter_us += jitter;
        if (jitter > t->worst_jitter_us)
            t->worst_jitter_us = jitter;

        set_gpio_value(t->gpio, 1);
        busy_compute(t->compute_ms);
        set_gpio_value(t->gpio, 0);

        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &finish);

        // Deadline miss check
        if (diff_us(finish, t->deadline) > 0) {
            t->deadline_misses++;
            deadline_latch_alarm(alarm, t->deadline_misses);
        }

        // Move to next window
        t->next_release = t->deadline;
        add_ms(&t->deadline, t->period_ms);

        t->jobs++;
        remaining--;
    }

    // Compute runtime for reporting
    clock_gettime(CLOCK_MONOTONIC, &end);
    double runtime_sec = diff_us(end, start) / 1e6;

    // Convert EDF stats to shared report format
    TaskStats stats[3];
    for (int i = 0; i < 3; i++) {
        stats[i].name            = tasks[i].name;
        stats[i].worst_jitter_us = tasks[i].worst_jitter_us;
        stats[i].total_jitter_us = tasks[i].total_jitter_us;
        stats[i].jobs            = tasks[i].jobs;
        stats[i].deadline_misses = tasks[i].deadline_misses;
    }

    print_scheduler_report("EDF (superloop, non-preemptive)", stats, 3, runtime_sec);
    reset_gpio_pins(g1, g2, g3, alarm);
    return 0;
}
