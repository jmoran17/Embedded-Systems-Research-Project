#include "gpio.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Helper to execute pinctrl commands.
// Used by all GPIO functions.
static int run_cmd(const char* cmd) {
    int ret = system(cmd);
    if (ret != 0)
        printf("GPIO command failed: %s\n", cmd);
    return ret;
}

// Raspberry Pi 5 uses "pinctrl", not /sys/class/gpio.
// This configures the pin as output.
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

// Drives the pin high/low.
// Used to toggle task LEDs and the alarm LED.
int set_gpio_value(int gpio, int value) {
    char cmd[80];
    snprintf(cmd, sizeof(cmd),
             value ? "pinctrl set %d dh" : "pinctrl set %d dl",
             gpio);
    return run_cmd(cmd);
}
