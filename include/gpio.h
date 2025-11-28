#ifndef GPIO_H
#define GPIO_H

int export_gpio(int gpio);
int set_gpio_direction(int gpio, const char* dir);
int set_gpio_value(int gpio, int value);

#endif
