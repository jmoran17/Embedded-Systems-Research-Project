#ifndef GPIO_H
#define GPIO_H

// Initialize a GPIO pin for output use on the Raspberry Pi 5 (via pinctrl)
int export_gpio(int gpio);

// Set a pin's direction (we only use "out" for driving LEDs)
int set_gpio_direction(int gpio, const char* dir);

// Drive a pin HIGH (1) or LOW (0)
int set_gpio_value(int gpio, int value);

#endif
