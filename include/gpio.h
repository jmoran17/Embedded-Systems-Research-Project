#ifndef GPIO_H
#define GPIO_H

// Initialize a GPIO pin for use
int export_gpio(int gpio);

// Set GPIO direction (we use "out")
int set_gpio_direction(int gpio, const char* dir);

// Set GPIO value: 1 = HIGH, 0 = LOW
int set_gpio_value(int gpio, int value);

#endif
