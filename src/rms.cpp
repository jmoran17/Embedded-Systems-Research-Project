#include "rms.h"
#include "gpio.h"
#include "scheduler_utils.h"

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <stdio.h>

// RMS Task structure.
// Represents a single periodic real-time task scheduled under SCHED_FIFO.
struct RMSTask {
    const char* name;          // Label used in printed output
    int gpio;                  // LED pin for visualizing execution
    long period_ms;            // Period of task (controls activation frequency)
    long compute_ms;           // Simulated WCET (workload)
    int priority;              // Fixed priority (shorter period = higher)

    // Runtime statistics
    long worst_jitter_us;
    long total_jitter_us;
    int deadline_misses;
    int jobs;

    struct timespec next_release; // Next activation time
    int max_jobs;                 // Total jobs to execute (passed from main)
};

static const int alarm_led = 5;
static RMSTask tasks[3];

// -----------------------------------------------------------------------------
// RMS Thread Function
// Each task runs in its own SCHED_FIFO thread, allowing true preemption.
// -----------------------------------------------------------------------------
void* rms_thread_fn(void* arg) {
    RMSTask* t = (RMSTask*)arg;

    // Apply real-time priority via SCHED_FIFO
    struct sched_param p;
    p.sched_priority = t->priority;
    if (sched_setscheduler(0, SCHED_FIFO, &p) != 0)
        perror("sched_setscheduler RMS");

    int led_state = 0;

    // Correct loop condition: run until completing all assigned jobs
    while (t->jobs < t->max_jobs) {

        // Sleep until the exact release time (absolute)
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t->next_release, NULL);

        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Compute start-time jitter (difference from scheduled release)
        long jitter = diff_us(start, t->next_release);
        if (jitter < 0) jitter = -jitter;

        t->total_jitter_us += jitter;
        if (jitter > t->worst_jitter_us)
            t->worst_jitter_us = jitter;

        // Compute absolute deadline = release time + period
        struct timespec deadline = t->next_release;
        add_ms(&deadline, t->period_ms);

        // Visual: toggle LED to show task execution
        led_state = !led_state;
        set_gpio_value(t->gpio, led_state);

        // Execute WCET workload
        busy_compute(t->compute_ms);

        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &finish);

        // Deadline checking + latch alarm LED on first miss
        if (diff_us(finish, deadline) > 0) {
            t->deadline_misses++;
            deadline_latch_alarm(alarm_led, t->deadline_misses);
        }

        // Advance next release by exactly one period
        add_ms(&t->next_release, t->period_ms);

        t->jobs++;
    }

    // Ensure LED for this task is turned off at end
    set_gpio_value(t->gpio, 0);
    return NULL;
}

// -----------------------------------------------------------------------------
// PUBLIC: run_rms()
// Initializes tasks, spawns SCHED_FIFO threads, collects stats.
// -----------------------------------------------------------------------------
int run_rms(int jobs_per_task) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int g1 = 17, g2 = 27, g3 = 22;
    init_gpio_pins(g1, g2, g3, alarm_led);

    // RMS priorities follow rate monotonic rules:
    // shorter period = higher static priority
    tasks[0] = {"T1_10ms",  g1, 10, 1, 90};
    tasks[1] = {"T2_50ms",  g2, 50, 2, 70};
    tasks[2] = {"T3_100ms", g3, 100, 3, 60};

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // Initialize per-task timing + counters
    for (int i = 0; i < 3; i++) {
        tasks[i].next_release    = now;
        tasks[i].worst_jitter_us = 0;
        tasks[i].total_jitter_us = 0;
        tasks[i].deadline_misses = 0;
        tasks[i].jobs            = 0;

        // ❗ REQUIRED FIX — Assign max_jobs properly
        tasks[i].max_jobs        = jobs_per_task;
    }

    // Spawn 3 preemptive RT threads
    pthread_t threads[3];
    for (int i = 0; i < 3; i++)
        pthread_create(&threads[i], NULL, rms_thread_fn, &tasks[i]);

    for (int i = 0; i < 3; i++)
        pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double runtime_sec = diff_us(t1, t0) / 1e6;

    // Convert results into shared presentation format
    TaskStats stats[3];
    for (int i = 0; i < 3; i++) {
        stats[i].name            = tasks[i].name;
        stats[i].worst_jitter_us = tasks[i].worst_jitter_us;
        stats[i].total_jitter_us = tasks[i].total_jitter_us;
        stats[i].jobs            = tasks[i].jobs;
        stats[i].deadline_misses = tasks[i].deadline_misses;
    }

    print_scheduler_report("RMS (preemptive SCHED_FIFO)", stats, 3, runtime_sec);

    reset_gpio_pins(g1, g2, g3, alarm_led);
    return 0;
}
