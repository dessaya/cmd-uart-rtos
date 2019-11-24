#ifndef GPIO_BLINK_H
#define GPIO_BLINK_H

#include "gpio.h"

/** `gpio <port> blink` command handler function. */
void gpio_blink_cmd_handler(gpio_port_t *port, cmd_args_t *args);

#endif
