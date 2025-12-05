#include "gpio.h"
#include "rms.h"
#include "edf.h"

#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ------- Helpers and structs for DEFAULT (kernel) scheduling -------

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

struct TaskInfo {
    const char* name;
    int gpio;
    long period_ms;
    long compute_ms;

    long worst_jitter;
    long total_jitter;
    int jobs;
    int deadline_misses;
    int max_jobs;
    struct timespec next_release;
};

void* task_thread(void* arg) {
    TaskInfo* t = (TaskInfo*)arg;

    t->worst_jitter = 0;
    t->total_jitter = 0;
    t->jobs = 0;
    t->deadline_misses = 0;

    int led_state = 0;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    t->next_release = now;

    while (t->jobs < t->max_jobs) {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t->next_release, NULL);

        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        long jitter = diff_us(start, t->next_release);
        if (jitter < 0) jitter = -jitter;
        t->total_jitter += jitter;
        if (jitter > t->worst_jitter) t->worst_jitter = jitter;

          // ----- Jitter Alarm LED -----
        // If jitter is greater than threshold_us, turn LED on
        long threshold_us = 2500; // 1ms threshold
       if (t->deadline_misses > 0) {
            set_gpio_value(5, 1);   // alarm ON
       } else {
        set_gpio_value(5,0);
       }

        led_state = !led_state;
        set_gpio_value(t->gpio, led_state);

        busy_compute(t->compute_ms);

        struct timespec finish;
        clock_gettime(CLOCK_MONOTONIC, &finish);

        struct timespec deadline = t->next_release;
        add_ms(&deadline, t->period_ms);

        if (diff_us(finish, deadline) > 0)
            t->deadline_misses++;

        add_ms(&t->next_release, t->period_ms);
        t->jobs++;
    }

    set_gpio_value(t->gpio, 0);
    return NULL;
}

static void run_default_kernel_demo(int periods) {
    // Measure total run time
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

    TaskInfo t1;
    t1.name = "T1_10ms";
    t1.gpio = g1;
    t1.period_ms = 10;
    t1.compute_ms = 1;
    t1.max_jobs = periods;

    TaskInfo t2;
    t2.name = "T2_50ms";
    t2.gpio = g2;
    t2.period_ms = 50;
    t2.compute_ms = 2;
    t2.max_jobs = periods;

    TaskInfo t3;
    t3.name = "T3_100ms";
    t3.gpio = g3;
    t3.period_ms = 100;
    t3.compute_ms = 3;
    t3.max_jobs = periods;

    pthread_t th1, th2, th3;

    pthread_create(&th1, NULL, task_thread, &t1);
    pthread_create(&th2, NULL, task_thread, &t2);
    pthread_create(&th3, NULL, task_thread, &t3);

    pthread_join(th1, NULL);
    pthread_join(th2, NULL);
    pthread_join(th3, NULL);

    printf("=== DEFAULT (KERNEL) SCHEDULING RESULTS ===\n\n");

    double avg1 = (double)t1.total_jitter / t1.jobs / 1000.0;
    double avg2 = (double)t2.total_jitter / t2.jobs / 1000.0;
    double avg3 = (double)t3.total_jitter / t3.jobs / 1000.0;

    printf("%s:\n", t1.name);
    printf("  Worst jitter: %.3f ms\n", t1.worst_jitter / 1000.0);
    printf("  Avg jitter:   %.3f ms\n", avg1);
    printf("  Deadline misses: %d\n\n", t1.deadline_misses);

    printf("%s:\n", t2.name);
    printf("  Worst jitter: %.3f ms\n", t2.worst_jitter / 1000.0);
    printf("  Avg jitter:   %.3f ms\n", avg2);
    printf("  Deadline misses: %d\n\n", t2.deadline_misses);

    printf("%s:\n", t3.name);
    printf("  Worst jitter: %.3f ms\n", t3.worst_jitter / 1000.0);
    printf("  Avg jitter:   %.3f ms\n", avg3);
    printf("  Deadline misses: %d\n\n", t3.deadline_misses);

        // ---- Total stats for default/kernel mode ----
    int total_misses = t1.deadline_misses + t2.deadline_misses + t3.deadline_misses;
    double avg_misses_per_task = (double)total_misses / 3.0;

    clock_gettime(CLOCK_MONOTONIC, &global_end);
    long total_us = diff_us(global_end, global_start);
    double total_sec = (double)total_us / 1000000.0;

    printf("TOTAL deadline misses (all tasks): %d\n", total_misses);
    printf("Average deadline misses per task: %.2f\n", avg_misses_per_task);
    printf("Total run time: %.3f seconds\n\n", total_sec);


    // Turn off LEDs
    set_gpio_value(g1, 0);
    set_gpio_value(g2, 0);
    set_gpio_value(g3, 0);
    set_gpio_value(jitter_led, 0);

}

// --------- Main: choose mode (default / rms / edf) ---------

static void print_usage(const char* prog) {
    printf("Usage:\n");
    printf("  %s --mode default --periods N\n", prog);
    printf("  %s --mode rms     --jobs N\n", prog);
    printf("  %s --mode edf     --jobs N\n", prog);
    printf("\n");
    printf("Examples:\n");
    printf("  sudo %s --mode default --periods 1000\n", prog);
    printf("  sudo chrt -f 90 %s --mode default --periods 1000   (FIFO)\n", prog);
    printf("  sudo chrt -r 80 %s --mode default --periods 1000   (RR)\n", prog);
    printf("  sudo %s --mode rms --jobs 300\n", prog);
    printf("  sudo %s --mode edf --jobs 300\n", prog);
}

int main(int argc, char** argv) {
    const char* mode = "default";
    int periods = 1000;
    int jobs    = 300;

    int i = 1;
    while (i + 1 < argc) {
        if (strcmp(argv[i], "--mode") == 0) {
            mode = argv[i + 1];
        } else if (strcmp(argv[i], "--periods") == 0) {
            periods = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "--jobs") == 0) {
            jobs = atoi(argv[i + 1]);
        } else {
            print_usage(argv[0]);
            return 1;
        }
        i += 2;
    }

    if (strcmp(mode, "default") == 0) {
        printf("Running DEFAULT (kernel) scheduling demo...\n\n");
        run_default_kernel_demo(periods);
    } else if (strcmp(mode, "rms") == 0) {
        printf("Running RMS (embedded) scheduler...\n\n");
        run_rms(jobs);
    } else if (strcmp(mode, "edf") == 0) {
        printf("Running EDF (embedded) scheduler...\n\n");
        run_edf(jobs);
    } else {
        printf("Unknown mode: %s\n\n", mode);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
