#include "gpio.h"
#include "rms.h"
#include "edf.h"
#include "scheduler_utils.h"

#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Default scheduler task using Linux CFS.
// Runs in separate pthreads to show non-real-time behavior.
struct DefaultTask {
    const char* name;
    int gpio;
    long period_ms;
    long compute_ms;

    long worst_jitter_us;
    long total_jitter_us;
    int deadline_misses;
    int jobs;

    struct timespec next_release;
    int max_jobs;
};

static const int alarm_gpio = 5;
static DefaultTask tasks[3];

void* default_task_fn(void* arg) {
    DefaultTask* t = (DefaultTask*)arg;
    int led_state = 0;

    while (t->jobs < t->max_jobs) {
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

        led_state = !led_state;
        set_gpio_value(t->gpio, led_state);

        busy_compute(t->compute_ms);

        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &finish);

        if (diff_us(finish, deadline) > 0) {
            t->deadline_misses++;
            deadline_latch_alarm(alarm_gpio, t->deadline_misses);
        }

        add_ms(&t->next_release, t->period_ms);
        t->jobs++;
    }

    set_gpio_value(t->gpio, 0);
    return NULL;
}

void run_default(int periods) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int g1 = 17, g2 = 27, g3 = 22;
    init_gpio_pins(g1, g2, g3, alarm_gpio);

    // Same 3-task workload as the RT schedulers.
    tasks[0] = {"T1_10ms",  g1, 10, 1, 0, 0, 0, 0, {}, periods};
    tasks[1] = {"T2_50ms",  g2, 50, 2, 0, 0, 0, 0, {}, periods};
    tasks[2] = {"T3_100ms", g3, 100, 3, 0, 0, 0, 0, {}, periods};

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    for (int i = 0; i < 3; i++) {
        tasks[i].next_release = now;
    }

    pthread_t th[3];
    for (int i = 0; i < 3; i++)
        pthread_create(&th[i], NULL, default_task_fn, &tasks[i]);

    for (int i = 0; i < 3; i++)
        pthread_join(th[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double runtime_sec = diff_us(t1, t0) / 1e6;

    // Collect stats in unified format
    TaskStats stats[3];
    for (int i = 0; i < 3; i++) {
        stats[i].name            = tasks[i].name;
        stats[i].worst_jitter_us = tasks[i].worst_jitter_us;
        stats[i].total_jitter_us = tasks[i].total_jitter_us;
        stats[i].jobs            = tasks[i].jobs;
        stats[i].deadline_misses = tasks[i].deadline_misses;
    }

    print_scheduler_report("DEFAULT (Linux CFS)", stats, 3, runtime_sec);
    reset_gpio_pins(g1, g2, g3, alarm_gpio);
}

static void print_usage(const char* prog) {
    printf("Usage:\n");
    printf("  %s --mode default --periods N\n", prog);
    printf("  %s --mode rms     --jobs N\n",    prog);
    printf("  %s --mode edf     --jobs N\n",    prog);
}

int main(int argc, char** argv) {
    const char* mode = "default";
    int periods = 1000;
    int jobs    = 300;

    int i = 1;
    while (i + 1 < argc) {
        if (!strcmp(argv[i], "--mode"))       mode    = argv[i + 1];
        else if (!strcmp(argv[i], "--periods")) periods = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--jobs"))     jobs    = atoi(argv[i + 1]);
        else { print_usage(argv[0]); return 1; }
        i += 2;
    }

    if (!strcmp(mode, "default")) run_default(periods);
    else if (!strcmp(mode, "rms")) run_rms(jobs);
    else if (!strcmp(mode, "edf")) run_edf(jobs);
    else { print_usage(argv[0]); return 1; }

    return 0;
}
