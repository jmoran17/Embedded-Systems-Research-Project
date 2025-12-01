#include "rms.h"
#include "gpio.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

// Time difference in microseconds
static long diff_us(struct timespec a, struct timespec b) {
    long sec  = a.tv_sec  - b.tv_sec;
    long nsec = a.tv_nsec - b.tv_nsec;
    return sec * 1000000L + nsec / 1000L;
}

// Busy wait to simulate work
static void busy_compute(long ms) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    long target = ms * 1000L;

    do {
        clock_gettime(CLOCK_MONOTONIC, &now);
    } while (diff_us(now, start) < target);
}

// Add ms to timespec
static void add_ms(struct timespec* t, long ms) {
    t->tv_sec  += ms / 1000;
    t->tv_nsec += (ms % 1000) * 1000000L;

    if (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        t->tv_sec  += 1;
    }
}

// Task configuration data
struct TaskConf {
    const char* name;
    int gpio;
    long period_ms;
    long compute_ms;
};

// Task runtime state
struct TaskState {
    TaskConf cfg;
    struct timespec next_release;
    struct timespec deadline;
    int led_state;

    long worst_jitter;
    long total_jitter;
    int jobs;
    int misses;
};

int run_rms(int jobs_per_task) {

    struct timespec global_start, global_end;
    clock_gettime(CLOCK_MONOTONIC, &global_start);

    // GPIO pins
    int g1 = 17;
    int g2 = 27;
    int g3 = 22;
    int jitter_led = 5;


    // Setup pins all start off
    export_gpio(g1); set_gpio_direction(g1, "out"); set_gpio_value(g1, 0);
    export_gpio(g2); set_gpio_direction(g2, "out"); set_gpio_value(g2, 0);
    export_gpio(g3); set_gpio_direction(g3, "out"); set_gpio_value(g3, 0);
    export_gpio(jitter_led);set_gpio_direction(jitter_led, "out");set_gpio_value(jitter_led, 0); 

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // 3 periodic tasks
    TaskState T[3];

    T[0].cfg = {"T1_10ms",  g1, 10, 1};
    T[1].cfg = {"T2_50ms",  g2, 50, 2};
    T[2].cfg = {"T3_100ms", g3, 100, 3};

    // Init timing state
    for (int i = 0; i < 3; i++) {
        T[i].next_release = now;
        T[i].deadline     = now;
        add_ms(&T[i].deadline, T[i].cfg.period_ms);

        T[i].led_state = 0;
        T[i].worst_jitter = 0;
        T[i].total_jitter = 0;
        T[i].jobs = 0;
        T[i].misses = 0;
    }

    int total_jobs_left = jobs_per_task * 3;

    // RMS: choose READY task with SHORTEST PERIOD
    while (total_jobs_left > 0) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        int best = -1;
        long best_period = 0;

        // Choose highest priority ready task
        for (int i = 0; i < 3; i++) {
            if (T[i].jobs >= jobs_per_task) continue;

            if (diff_us(now, T[i].next_release) >= 0) {
                if (best == -1 || T[i].cfg.period_ms < best_period) {
                    best = i;
                    best_period = T[i].cfg.period_ms;
                }
            }
        }

        // If none ready, sleep until earliest next release
        if (best == -1) {
            struct timespec earliest = T[0].next_release;
            for (int i = 1; i < 3; i++) {
                if (T[i].jobs < jobs_per_task &&
                    diff_us(T[i].next_release, earliest) < 0) {
                    earliest = T[i].next_release;
                }
            }
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &earliest, NULL);
            continue;
        }

        TaskState* t = &T[best];

        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Jitter: difference between actual and ideal start
        long jitter = diff_us(start, t->next_release);
        if (jitter < 0) jitter = -jitter;
        t->total_jitter += jitter;
        if (jitter > t->worst_jitter) t->worst_jitter = jitter;

        // ----- Jitter Alarm LED -----
        // If jitter is greater than threshold_us, turn LED on
        long threshold_us = 1000; // 1ms threshold
        if (jitter > threshold_us) {
            set_gpio_value(5, 1);   // alarm ON
        } else {
            set_gpio_value(5, 0);   // alarm OFF
        }


        // Toggle LED
        t->led_state = !t->led_state;
        set_gpio_value(t->cfg.gpio, t->led_state);

        // Do work
        busy_compute(t->cfg.compute_ms);

        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &finish);

        // Deadline check
        if (diff_us(finish, t->deadline) > 0) {
            t->misses++;
        }

        // Schedule next job
        t->next_release = t->deadline;
        add_ms(&t->deadline, t->cfg.period_ms);

        t->jobs++;
        total_jobs_left--;
    }

    // Turn off LEDs
    set_gpio_value(g1, 0);
    set_gpio_value(g2, 0);
    set_gpio_value(g3, 0);
    set_gpio_value(jitter_led, 0);

    // Print stats
    for (int i = 0; i < 3; i++) {
        double avg_ms = (double)T[i].total_jitter / T[i].jobs / 1000.0;
        printf("%s:\n", T[i].cfg.name);
        printf("  Worst jitter: %.3f ms\n", T[i].worst_jitter / 1000.0);
        printf("  Avg jitter:   %.3f ms\n", avg_ms);
        printf("  Deadline misses: %d\n\n", T[i].misses);

            // ---- Total stats for RMS ----
    int total_misses = 0;
    for (int i = 0; i < 3; i++) {
        total_misses += T[i].misses;
    }
    double avg_misses_per_task = (double)total_misses / 3.0;

    clock_gettime(CLOCK_MONOTONIC, &global_end);
    long total_us = diff_us(global_end, global_start);
    double total_sec = (double)total_us / 1000000.0;

    printf("RMS - TOTAL deadline misses (all tasks): %d\n", total_misses);
    printf("RMS - Average deadline misses per task: %.2f\n", avg_misses_per_task);
    printf("RMS - Total run time: %.3f seconds\n\n", total_sec);

    
}
    return 0;
}

    
