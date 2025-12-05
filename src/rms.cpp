#include "rms.h"
#include "gpio.h"

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -------- Helpers --------

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

// -------- RMS Task --------

struct RMSTask {
    const char* name;
    int gpio;
    long period_ms;
    long compute_ms;
    int priority;            // SCHED_FIFO priority
    int max_jobs;

    long worst_jitter;
    long total_jitter;
    int deadline_misses;
    int jobs;

    struct timespec next_release;
};

static RMSTask tasks[3];
static int jitter_led = 5;

void* rms_thread_fn(void* arg) {
    RMSTask* t = (RMSTask*)arg;

    // Set SCHED_FIFO priority
    struct sched_param p;
    p.sched_priority = t->priority;
    if (sched_setscheduler(0, SCHED_FIFO, &p) != 0) {
        perror("sched_setscheduler RMS");
    }

    t->worst_jitter = 0;
    t->total_jitter = 0;
    t->deadline_misses = 0;
    t->jobs = 0;

    int led_state = 0;

    while (t->jobs < t->max_jobs) {

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t->next_release, NULL);

        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        long jitter = diff_us(start, t->next_release);
        if (jitter < 0) jitter = -jitter;

        t->total_jitter += jitter;
        if (jitter > t->worst_jitter) t->worst_jitter = jitter;

        // Deadline
        struct timespec deadline = t->next_release;
        add_ms(&deadline, t->period_ms);

        // Toggle LED
        led_state = !led_state;
        set_gpio_value(t->gpio, led_state);

        busy_compute(t->compute_ms);

        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &finish);

        if (diff_us(finish, deadline) > 0)
            t->deadline_misses++;

        // Next release
        add_ms(&t->next_release, t->period_ms);

        t->jobs++;
    }

    set_gpio_value(t->gpio, 0);
    return NULL;
}

// -------- PUBLIC: run_rms() --------

int run_rms(int jobs_per_task) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int g1 = 17, g2 = 27, g3 = 22;
    jitter_led = 5;

    export_gpio(g1); set_gpio_direction(g1, "out"); set_gpio_value(g1, 0);
    export_gpio(g2); set_gpio_direction(g2, "out"); set_gpio_value(g2, 0);
    export_gpio(g3); set_gpio_direction(g3, "out"); set_gpio_value(g3, 0);
    export_gpio(jitter_led); set_gpio_direction(jitter_led, "out"); set_gpio_value(jitter_led, 0);

    // RMS priority assignment (shorter period = higher priority)
    tasks[0] = {"T1_10ms",  g1, 10, 1, 90, jobs_per_task};
    tasks[1] = {"T2_50ms",  g2, 50, 2, 70, jobs_per_task};
    tasks[2] = {"T3_100ms", g3, 100, 3, 60, jobs_per_task};

    // Common start time
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    for (int i = 0; i < 3; i++) {
        tasks[i].next_release = now;
    }

    pthread_t threads[3];
    for (int i = 0; i < 3; i++)
        pthread_create(&threads[i], NULL, rms_thread_fn, &tasks[i]);

    for (int i = 0; i < 3; i++)
        pthread_join(threads[i], NULL);

    // Stats
    printf("=== RMS (preemptive SCHED_FIFO) ===\n\n");

    int total_misses = 0;
    for (int i = 0; i < 3; i++) {
        double avg = (double)tasks[i].total_jitter / tasks[i].jobs / 1000.0;

        printf("%s:\n", tasks[i].name);
        printf("  Worst jitter: %.3f ms\n", tasks[i].worst_jitter / 1000.0);
        printf("  Avg jitter: %.3f ms\n", avg);
        printf("  Deadline misses: %d\n\n", tasks[i].deadline_misses);

        total_misses += tasks[i].deadline_misses;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("RMS Total deadline misses: %d\n", total_misses);
    printf("Runtime: %.3f sec\n\n",
           diff_us(end, start) / 1e6);

    set_gpio_value(jitter_led, 0);
    return 0;
}
