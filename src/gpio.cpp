#include "gpio.h"

#include <stdlib.h>   // system
#include <stdio.h>    // printf
#include <string.h>   // strcmp

// Helper to run a shell command
static int run_cmd(const char* cmd) {
    int ret = system(cmd);
    if (ret != 0) {
        printf("GPIO command failed: %s\n", cmd);
    }
    return ret;
}

// On Pi 5 we use pinctrl, not /sys/class/gpio
int export_gpio(int gpio) {
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "pinctrl set %d op", gpio);
    return run_cmd(cmd);
}

int set_gpio_direction(int gpio, const char* dir) {
    if (strcmp(dir, "out") == 0) {
        char cmd[80];
        snprintf(cmd, sizeof(cmd), "pinctrl set %d op", gpio);
        return run_cmd(cmd);
    }
    return 0;
}

int set_gpio_value(int gpio, int value) {
    char cmd[80];
    if (value) {
        snprintf(cmd, sizeof(cmd), "pinctrl set %d dh", gpio); // drive high
    } else {
        snprintf(cmd, sizeof(cmd), "pinctrl set %d dl", gpio); // drive low
    }
    return run_cmd(cmd);
}
