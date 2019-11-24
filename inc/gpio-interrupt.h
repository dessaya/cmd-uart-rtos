#ifndef GPIO_INTERRUPT_H
#define GPIO_INTERRUPT_H

#include "gpio.h"

/** `gpio <port> interrupt` command handler function. */
void gpio_interrupt_cmd_handler(gpio_port_t *port, cmd_args_t *args);

#endif
