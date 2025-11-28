#include "gpio.h"

#include <stdlib.h>   // system()
#include <stdio.h>
#include <string.h>

static int run_cmd(const char* cmd) {
    int ret = system(cmd);
    if (ret != 0) {
        printf("GPIO command failed: %s (ret=%d)\n", cmd, ret);
    }
    return ret;
}

int export_gpio(int gpio) {
    // On Pi 5 we don't "export" via /sys/class/gpio.
    // Instead, we just set the pin mode using pinctrl.
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "pinctrl set %d op", gpio);  // op = output
    return run_cmd(cmd);
}

int set_gpio_direction(int gpio, const char* dir) {
    // We only care about "out" in this project.
    if (strcmp(dir, "out") == 0) {
        char cmd[80];
        snprintf(cmd, sizeof(cmd), "pinctrl set %d op", gpio);
        return run_cmd(cmd);
    }

    // You could add "in" handling here if needed.
    return 0;
}

int set_gpio_value(int gpio, int value) {
    char cmd[80];

    if (value) {
        // dh = drive high
        snprintf(cmd, sizeof(cmd), "pinctrl set %d dh", gpio);
    } else {
        // dl = drive low
        snprintf(cmd, sizeof(cmd), "pinctrl set %d dl", gpio);
    }

    return run_cmd(cmd);
}
