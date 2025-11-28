#include "tasks.h"
#include "gpio.h"

#include <pthread.h>
#include <time.h>
#include <stdio.h>

static long diff_us(struct timespec a, struct timespec b) {
    long sec  = a.tv_sec  - b.tv_sec;
    long nsec = a.tv_nsec - b.tv_nsec;
    return sec * 1000000L + nsec / 1000L;
}

static void busy_compute(long ms) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    long target_us = ms * 1000L;
    do {
        clock_gettime(CLOCK_MONOTONIC, &now);
    } while (diff_us(now, start) < target_us);
}

void* periodic_task(void* arg) {
    TaskConf* cfg = (TaskConf*)arg;

    long worst_jitter = 0;
    long total_jitter = 0;
    int deadline_misses = 0;

    set_gpio_value(cfg->gpio, 0);
    int led_state = 0;

    struct timespec next_release;
    clock_gettime(CLOCK_MONOTONIC, &next_release);

    long period_us = cfg->period_ms * 1000L;

    for (int i = 0; i < cfg->iterations; i++) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_release, NULL);

        struct timespec actual;
        clock_gettime(CLOCK_MONOTONIC, &actual);

        long jitter_us = diff_us(actual, next_release);
        if (jitter_us < 0) jitter_us = -jitter_us;

        total_jitter += jitter_us;
        if (jitter_us > worst_jitter) worst_jitter = jitter_us;

        led_state = !led_state;
        set_gpio_value(cfg->gpio, led_state);

        busy_compute(cfg->compute_ms);

        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &finish);

        struct timespec deadline = next_release;
        long add_ns = (period_us % 1000000L) * 1000L;
        deadline.tv_nsec += add_ns;
        deadline.tv_sec  += period_us / 1000000L;

        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_nsec -= 1000000000L;
            deadline.tv_sec  += 1;
        }

        if (diff_us(finish, deadline) > 0) {
            deadline_misses++;
        }

        next_release = deadline;
    }

    double avg_ms =
        (double)total_jitter / (double)cfg->iterations / 1000.0;

    printf("%s (GPIO %d)\n", cfg->name, cfg->gpio);
    printf("  Worst jitter: %.3f ms\n", worst_jitter / 1000.0);
    printf("  Avg jitter:   %.3f ms\n", avg_ms);
    printf("  Deadline misses: %d\n\n", deadline_misses);

    return NULL;
}
