#include <string.h>
#include <stdlib.h>
#include "gpio.h"
#include "terminal.h"
#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_priorities.h"

static void usage() {
    terminal_puts(
        "Usage: gpio <pin> <command> ...\r\n"
        "Examples:\r\n"
        "  gpio TEC1 read\r\n"
        "  gpio LEDB write 1\r\n"
        "  gpio LED1 blink 2000\r\n"
    );
}

#define CMD_ASSERT_USAGE(cond) do { \
    if (!(cond)) { \
        usage(); \
        return; \
    } \
} while (0)

typedef struct {
    char *name;
    gpioMap_t pin;
    TaskHandle_t blink_task;
    char *blink_task_name;
} port_t;

#define MAKE_PORT(gpio_port) { \
    .name = #gpio_port, \
    .pin = gpio_port, \
    .blink_task = 0, \
    .blink_task_name = #gpio_port " blink task" \
}

static port_t ports[] = {
    MAKE_PORT(TEC1),
    MAKE_PORT(TEC2),
    MAKE_PORT(TEC3),
    MAKE_PORT(TEC4),

    MAKE_PORT(LEDR),
    MAKE_PORT(LEDG),
    MAKE_PORT(LEDB),
    MAKE_PORT(LED1),
    MAKE_PORT(LED2),
    MAKE_PORT(LED3),
    {0},
};

static port_t *find_port(const char *name) {
    for (port_t *p = ports; p->name; p++) {
        if (!strcmp(p->name, name)) {
            return p;
        }
    }
    return NULL;
}

typedef struct {
    char *name;
    bool_t value;
} value_token_t;

static value_token_t value_tokens[] = {
    {"high", HIGH},
    {"low", LOW},
    {"on", HIGH},
    {"off", LOW},
    {"1", HIGH},
    {"0", LOW},
    {0},
};

static char *value_to_string(bool_t value) {
    for (value_token_t *t = value_tokens; t->name; t++) {
        if (value == t->value) {
            return t->name;
        }
    }
    return NULL;
}

static value_token_t *find_value(const char *name) {
    for (value_token_t *t = value_tokens; t->name; t++) {
        if (!strcmp(t->name, name)) {
            return t;
        }
    }
    return NULL;
}

static void gpio_cmd_read_handler(port_t *port, cmd_args_t *args) {
    terminal_println(value_to_string(gpioRead(port->pin)));
}

static void gpio_cmd_write_handler(port_t *port, cmd_args_t *args) {
    CMD_ASSERT_USAGE(args->count >= 4);
    value_token_t *vt = find_value(args->tokens[3]);
    CMD_ASSERT_USAGE(vt);
    gpioWrite(port->pin, vt->value);
}

typedef struct {
    port_t *port;
    unsigned period;
} blink_task_param_t;

static void blink_task(void *param) {
    int period = ((blink_task_param_t *)param)->period;
    port_t *port = ((blink_task_param_t *)param)->port;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (true) {
        gpioToggle(port->pin);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(period / 2));
    }
}

static void gpio_cmd_blink_handler(port_t *port, cmd_args_t *args) {
    CMD_ASSERT_USAGE(args->count >= 4);
    int period = atoi(args->tokens[3]);
    CMD_ASSERT_USAGE(period >= 0);

    if (port->blink_task) {
        vTaskDelete(port->blink_task);
        port->blink_task = 0;
    }
    if (period > 0) {
        static blink_task_param_t param;
        param.port = port;
        param.period = period;
        if (xTaskCreate(
            blink_task,
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

typedef void (*gpio_cmd_handler_t)(port_t *port, cmd_args_t *args);

typedef struct {
    char **tokens;
    gpio_cmd_handler_t handler;
} gpio_cmd_token_t;

static gpio_cmd_token_t gpio_cmd_handlers[] = {
    {(char *[]){"r", "read", 0}, gpio_cmd_read_handler},
    {(char *[]){"w", "write", 0}, gpio_cmd_write_handler},
    {(char *[]){"w", "blink", 0}, gpio_cmd_blink_handler},
    {0},
};

static gpio_cmd_handler_t find_gpio_cmd(const char *name) {
    for (gpio_cmd_token_t *s = gpio_cmd_handlers; s->tokens; s++) {
        for (char **token = s->tokens; *token; token++) {
            if (!strcmp(*token, name)) {
                return s->handler;
            }
        }
    }
    return NULL;
}

static void gpio_cmd_handler(cmd_args_t *args) {
    CMD_ASSERT_USAGE(args->count >= 2);
    if (strcmp(args->tokens[1], "help")) {
        usage();
        return;
    }
    CMD_ASSERT_USAGE(args->count >= 3);
    port_t *port = find_port(args->tokens[1]);
    CMD_ASSERT_USAGE(port);
    gpio_cmd_handler_t command = find_gpio_cmd(args->tokens[2]);
    CMD_ASSERT_USAGE(command);
    command(port, args);
}

const cmd_t gpio_command = {
    .name = "gpio",
    .description = "Control GPIO ports",
    .handler = gpio_cmd_handler,
};
