#include "tasks.h"
#include "gpio.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {

    int iterations = 1000;
    if (argc == 3 && strcmp(argv[1], "--periods") == 0) {
        iterations = atoi(argv[2]);
    }

    int gpio1 = 17;
    int gpio2 = 27;
    int gpio3 = 22;

    export_gpio(gpio1); set_gpio_direction(gpio1, "out");
    export_gpio(gpio2); set_gpio_direction(gpio2, "out");
    export_gpio(gpio3); set_gpio_direction(gpio3, "out");

    TaskConf t1 = { "Task_10ms",  gpio1, 10,  2, iterations };
    TaskConf t2 = { "Task_50ms",  gpio2, 50,  5, iterations };
    TaskConf t3 = { "Task_100ms", gpio3, 100, 8, iterations };

    pthread_t th1, th2, th3;

    pthread_create(&th1, NULL, periodic_task, &t1);
    pthread_create(&th2, NULL, periodic_task, &t2);
    pthread_create(&th3, NULL, periodic_task, &t3);

    pthread_join(th1, NULL);
    pthread_join(th2, NULL);
    pthread_join(th3, NULL);

    set_gpio_value(gpio1, 0);
    set_gpio_value(gpio2, 0);
    set_gpio_value(gpio3, 0);

    return 0;
}
