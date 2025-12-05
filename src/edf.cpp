#include "edf.h"
#include "gpio.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

// ---- Helpers ----

static long diff_us(struct timespec a, struct timespec b) {
    long sec  = a.tv_sec  - b.tv_sec;
    long nsec = a.tv_nsec - b.tv_nsec;
    return sec * 1000000L + nsec / 1000L;
}

static void busy_compute(long ms) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    long target = ms * 1000L;
    do {
        clock_gettime(CLOCK_MONOTONIC, &now);
    } while (diff_us(now, start) < target);
}

static void add_ms(struct timespec* t, long ms) {
    t->tv_sec  += ms / 1000;
    t->tv_nsec += (ms % 1000) * 1000000L;
    if (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        t->tv_sec  += 1;
    }
}

// ---- EDF Task ----

struct EDFTask {
    const char* name;
    int gpio;
    long period_ms;
    long compute_ms;

    struct timespec next_release;
    struct timespec deadline;

    long worst_jitter;
    long total_jitter;
    int jobs;
    int misses;

    int max_jobs;
};

int run_edf(int jobs_per_task) {
    struct timespec global_start, global_end;
    clock_gettime(CLOCK_MONOTONIC, &global_start);

    int g1 = 17, g2 = 27, g3 = 22, jitter_led = 5;

    export_gpio(g1); set_gpio_direction(g1, "out"); set_gpio_value(g1, 0);
    export_gpio(g2); set_gpio_direction(g2, "out"); set_gpio_value(g2, 0);
    export_gpio(g3); set_gpio_direction(g3, "out"); set_gpio_value(g3, 0);
    export_gpio(jitter_led); set_gpio_direction(jitter_led, "out"); set_gpio_value(jitter_led, 0);

    EDFTask T[3];

    T[0] = {"T1_10ms", g1, 10, 1};
    T[1] = {"T2_50ms", g2, 50, 2};
    T[2] = {"T3_100ms", g3, 100, 3};

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    for (int i = 0; i < 3; i++) {
        T[i].next_release = now;
        T[i].deadline = now;
        add_ms(&T[i].deadline, T[i].period_ms);

        T[i].worst_jitter = 0;
        T[i].total_jitter = 0;
        T[i].jobs = 0;
        T[i].misses = 0;
        T[i].max_jobs = jobs_per_task;
    }

    int remaining = jobs_per_task * 3;

    while (remaining > 0) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        int best = -1;
        struct timespec best_dl;

        for (int i = 0; i < 3; i++) {
            if (T[i].jobs >= T[i].max_jobs) continue;

            if (diff_us(now, T[i].next_release) >= 0) {
                if (best == -1 || diff_us(T[i].deadline, best_dl) < 0) {
                    best = i;
                    best_dl = T[i].deadline;
                }
            }
        }

        if (best == -1) {
            struct timespec earliest = T[0].next_release;
            for (int i = 1; i < 3; i++) {
                if (T[i].jobs < T[i].max_jobs &&
                    diff_us(T[i].next_release, earliest) < 0) {
                    earliest = T[i].next_release;
                }
            }
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &earliest, NULL);
            continue;
        }

        EDFTask* t = &T[best];

        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        long jitter = diff_us(start, t->next_release);
        if (jitter < 0) jitter = -jitter;

        t->total_jitter += jitter;
        if (jitter > t->worst_jitter) t->worst_jitter = jitter;

        // Jitter LED only lights on deadline misses
        if (t->misses > 0) set_gpio_value(jitter_led, 1);

        set_gpio_value(t->gpio, 1);
        busy_compute(t->compute_ms);
        set_gpio_value(t->gpio, 0);

        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &finish);

        if (diff_us(finish, t->deadline) > 0)
            t->misses++;

        t->next_release = t->deadline;
        add_ms(&t->deadline, t->period_ms);

        t->jobs++;
        remaining--;
    }

    // Stats
    printf("=== EDF (non-preemptive single-threaded) ===\n\n");

    int total_misses = 0;
    for (int i = 0; i < 3; i++) {
        double avg = (double)T[i].total_jitter / T[i].jobs / 1000.0;

        printf("%s:\n", T[i].name);
        printf("  Worst jitter: %.3f ms\n", T[i].worst_jitter / 1000.0);
        printf("  Avg jitter: %.3f ms\n", avg);
        printf("  Deadline misses: %d\n\n", T[i].misses);

        total_misses += T[i].misses;
    }

    clock_gettime(CLOCK_MONOTONIC, &global_end);
    printf("EDF Total deadline misses: %d\n", total_misses);
    printf("Runtime: %.3f sec\n\n",
           diff_us(global_end, global_start) / 1e6);

    set_gpio_value(jitter_led, 0);
    return 0;
}
