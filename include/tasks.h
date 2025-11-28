#ifndef TASKS_H
#define TASKS_H

struct TaskConf {
    const char* name;
    int gpio;
    long period_ms;
    long compute_ms;
    int iterations;
};

void* periodic_task(void* arg);

#endif
