#include "rms.h"
#include "gpio.h"
#include "scheduler_utils.h"

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <stdio.h>

// RMS task: identical structure to EDF's but includes a fixed priority.
struct RMSTask {
    const char* name;
    int gpio;
    long period_ms;
    long compute_ms;
    int priority;              // SCHED_FIFO priority

    long worst_jitter_us;
    long total_jitter_us;
    int deadline_misses;
    int jobs;

    struct timespec next_release;
};

static const int alarm_led = 5;
static RMSTask tasks[3];

// RMS thread function: executes periodic workload under SCHED_FIFO.
void* rms_thread_fn(void* arg) {
    RMSTask* t = (RMSTask*)arg;

    // Enable real-time priority.
    struct sched_param p;
    p.sched_priority = t->priority;
    sched_setscheduler(0, SCHED_FIFO, &p);

    int led_state = 0;

    while (t->jobs < t->compute_ms) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t->next_release, NULL);

        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        long jitter = diff_us(start, t->next_release);
        if (jitter < 0) jitter = -jitter;
        t->total_jitter_us += jitter;
        if (jitter > t->worst_jitter_us)
            t->worst_jitter_us = jitter;

        struct timespec deadline = t->next_release;
        add_ms(&deadline, t->period_ms);

        // Toggle task LED to visualize execution.
        led_state = !led_state;
        set_gpio_value(t->gpio, led_state);

        busy_compute(t->compute_ms);

        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &finish);

        if (diff_us(finish, deadline) > 0) {
            t->deadline_misses++;
            deadline_latch_alarm(alarm_led, t->deadline_misses);
        }

        add_ms(&t->next_release, t->period_ms);
        t->jobs++;
    }

    set_gpio_value(t->gpio, 0);
    return NULL;
}

int run_rms(int jobs_per_task) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int g1 = 17, g2 = 27, g3 = 22;
    init_gpio_pins(g1, g2, g3, alarm_led);

    // Assign Rate-Monotonic priorities: shorter period → higher priority
    tasks[0] = {"T1_10ms",  g1, 10, 1, 90};
    tasks[1] = {"T2_50ms",  g2, 50, 2, 70};
    tasks[2] = {"T3_100ms", g3, 100, 3, 60};

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    for (int i = 0; i < 3; i++) {
        tasks[i].next_release    = now;
        tasks[i].worst_jitter_us = 0;
        tasks[i].total_jitter_us = 0;
        tasks[i].deadline_misses = 0;
        tasks[i].jobs            = 0;
    }

    pthread_t threads[3];
    for (int i = 0; i < 3; i++)
        pthread_create(&threads[i], NULL, rms_thread_fn, &tasks[i]);

    for (int i = 0; i < 3; i++)
        pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double runtime_sec = diff_us(t1, t0) / 1e6;

    // Convert RMS results into presentation format
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
