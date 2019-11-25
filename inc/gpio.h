#ifndef GPIO_H
#define GPIO_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cli.h"
#include "sapi.h"

/** `gpio` command definition. */
extern const cmd_t gpio_command;

/** Print the `gpio` command usage help. */
void gpio_usage();

/** GPIO port description and state. */
typedef struct {
    /** GPIO port name. */
    char *name;
    /** GPIO port pin number. */
    gpioMap_t pin;
    /** Name of the RTOS blink task. */
    char *blink_task_name;

    /** RTOS blink task handle for this GPIO port, or 0 if no blink task is currently active. */
    TaskHandle_t blink_task;

    /** Port for interrupt handler. */
    uint8_t chip_port;
    /** Pin for interrupt handler. */
    uint8_t chip_pin;

    /** Mutex to synchronize concurrent access. */
    SemaphoreHandle_t mutex;
} gpio_port_t;

/** Take the mutex for the given port. */
bool gpio_take_mutex(gpio_port_t *port, unsigned milliseconds);

/** Release the mutex for the given port. */
bool gpio_release_mutex(gpio_port_t *port);

#endif
