#include <string.h>
#include <stdlib.h>
#include "gpio.h"
#include "terminal.h"
#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_priorities.h"

typedef struct {
    char *name;
    gpioMap_t pin;
    TaskHandle_t blink_task;
    char *blink_task_name;
} port_t;

static port_t ports[] = {
    {"TEC1", TEC1, 0, "TEC1 blink task"},
    {"TEC2", TEC2, 0, "TEC2 blink task"},
    {"TEC3", TEC3, 0, "TEC3 blink task"},
    {"TEC4", TEC4, 0, "TEC4 blink task"},

    {"LEDR", LEDR, 0, "LEDR blink task"},
    {"LEDG", LEDG, 0, "LEDG blink task"},
    {"LEDB", LEDB, 0, "LEDB blink task"},
    {"LED1", LED1, 0, "LED1 blink task"},
    {"LED2", LED2, 0, "LED2 blink task"},
    {"LED3", LED3, 0, "LED3 blink task"},
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

typedef enum {READ, WRITE, BLINK} gpio_cmd_t;

typedef void (*gpio_cmd_handler_t)(port_t *port, cmd_args_t *args);

typedef struct {
    char **tokens;
    gpio_cmd_handler_t handler;
} gpio_cmd_token_t;

static void usage() {
    terminal_println("Usage: gpio <pin> <command> ...");
    terminal_println("Examples:");
    terminal_println("  gpio TEC1 read");
    terminal_println("  gpio LEDB write 1");
    terminal_println("  gpio LEDB blink 2000");
}

#define _CMD_ASSERT(cond, ...) do { \
    if (!(cond)) { \
        __VA_ARGS__; \
        return; \
    } \
} while (0)

#define CMD_ASSERT_USAGE(cond) _CMD_ASSERT(cond, usage())
#define CMD_ASSERT(cond) _CMD_ASSERT(cond, )

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
        if (!xTaskCreate(
            blink_task,
            port->blink_task_name,
            configMINIMAL_STACK_SIZE,
            &param,
            GPIO_BLINK_TASK_PRIORITY,
            &port->blink_task
        )) {
            terminal_println("Failed to create task");
        }
    }
}

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
    CMD_ASSERT_USAGE(args->count >= 3);
    port_t *port = find_port(args->tokens[1]);
    CMD_ASSERT_USAGE(port);
    gpio_cmd_handler_t command = find_gpio_cmd(args->tokens[2]);
    CMD_ASSERT_USAGE(command);
    command(port, args);
}

cmd_t gpio_command = {
    .cmd = "gpio",
    .description = "Control GPIO ports",
    .handler = gpio_cmd_handler,
    .next = NULL,
};
