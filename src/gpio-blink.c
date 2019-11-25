#include <stdlib.h>
#include "gpio-blink.h"
#include "task_priorities.h"
#include "terminal.h"

/** Parameters for the RTOS blink task. */
typedef struct {
    /** GPIO port to blink. */
    gpio_port_t *port;
    /** Blink period in ms. */
    unsigned period;
} blink_task_param_t;

/** RTOS task for controlling a blinking GPIO port. */
static void gpio_blink_task(void *param) {
    gpio_port_t *port = ((blink_task_param_t *)param)->port;
    int period = ((blink_task_param_t *)param)->period;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (true) {
        if (gpio_take_mutex(port, period / 4)) {
            gpioToggle(port->pin);
            gpio_release_mutex(port);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(period / 2));
    }
}

void gpio_blink_cmd_handler(gpio_port_t *port, cmd_args_t *args) {
    cli_assert(args->count == 4, gpio_usage);

    int period = atoi(args->tokens[3]);
    cli_assert(period >= 0, gpio_usage);

    if (port->blink_task) {
        vTaskDelete(port->blink_task);
        port->blink_task = 0;
    }
    if (period > 0) {
        static blink_task_param_t param;
        param.port = port;
        param.period = period;
        if (xTaskCreate(
            gpio_blink_task,
            port->blink_task_name,
            configMINIMAL_STACK_SIZE,
            &param,
            GPIO_BLINK_TASK_PRIORITY,
            &port->blink_task
        ) != pdPASS) {
            log_error("Failed to create task");
        }
    }
}

