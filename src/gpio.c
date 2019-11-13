#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "gpio.h"
#include "terminal.h"
#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_priorities.h"

/** Print the `gpio` command usage help. */
static void usage() {
    terminal_puts(
        "Usage: gpio <pin> <command> ...\r\n"
        "Examples:\r\n"
        "  gpio TEC1 read\r\n"
        "  gpio LEDB write 1\r\n"
        "  gpio LED1 blink 2000\r\n"
    );
}

/** Utility macro: check a condition; if it's false, print the command usage help and return. */
#define CMD_ASSERT_USAGE(cond) do { \
    if (!(cond)) { \
        usage(); \
        return; \
    } \
} while (0)

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
} port_t;

/** Utility macro to initialize the ports list. */
#define MAKE_PORT(gpio_port) { \
    .name = #gpio_port, \
    .pin = gpio_port, \
    .blink_task_name = #gpio_port " blink task", \
    .blink_task = 0, \
}

/** List of supported GPIO ports. */
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

/**
 * Find a port_t given its name.
 *
 * \return the port, or NULL if not found.
 */
static port_t *find_port(const char *name) {
    for (port_t *p = ports; p->name; p++) {
        if (!strcmp(p->name, name)) {
            return p;
        }
    }
    return NULL;
}

/**
 * Describes the different ways the user can turn a GPIO port on or off.
 *
 * Eg: `gpio LED1 write 1` and `gpio LED1 write high` are equivalent.
 */
static const char *on_off_tokens[][4] = {
    [HIGH] = {"high", "on", "1", 0},
    [LOW] = {"low", "off", "0", 0},
};

/** Return a string representing a GPIO on/off value. */
static const char *on_off_to_string(bool_t value) {
    assert(value == HIGH || value == LOW);
    return on_off_tokens[value][0];
}

/**
 * Parse a GPIO on/off value.
 *
 * \return false if the name does not correspond to an on/off value.
 */
static bool parse_on_off_value(const char *name, bool_t *out) {
    for (bool_t on_off = LOW; on_off <= HIGH; on_off++) {
        for (const char **s = on_off_tokens[on_off]; *s; s++) {
            if (!strcmp(*s, name)) {
                *out = on_off;
                return true;
            }
        }
    }
    return false;
}

/** `gpio <port> read` command handler function. */
static void gpio_cmd_read_handler(port_t *port, cmd_args_t *args) {
    terminal_println(on_off_to_string(gpioRead(port->pin)));
}

/** `gpio <port> write` command handler function. */
static void gpio_cmd_write_handler(port_t *port, cmd_args_t *args) {
    CMD_ASSERT_USAGE(args->count >= 4);
    bool_t on_off;
    CMD_ASSERT_USAGE(parse_on_off_value(args->tokens[3], &on_off));
    gpioWrite(port->pin, on_off);
}

/** Parameters for the RTOS blink task. */
typedef struct {
    /** GPIO port to blink. */
    port_t *port;
    /** Blink period in ms. */
    unsigned period;
} blink_task_param_t;

/** RTOS task for controlling a blinking GPIO port. */
static void blink_task(void *param) {
    port_t *port = ((blink_task_param_t *)param)->port;
    int period = ((blink_task_param_t *)param)->period;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (true) {
        gpioToggle(port->pin);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(period / 2));
    }
}

/** `gpio <port> blink` command handler function. */
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

/** `gpio <port> <subcommand>` handler function interface. */
typedef void (*gpio_cmd_handler_t)(port_t *port, cmd_args_t *args);

/** `gpio <port> <subcommand>` definition. */
typedef struct {
    /**
     * Accepted tokens for `<subcommand>`.
     *
     * Eg: this allows the user to call `gpio LED1 read` or simply `gpio LED1 r`.
     */
    char **tokens;

    /** Subcommand handler function. */
    gpio_cmd_handler_t handler;
} gpio_cmd_token_t;

/** List of `gpio` subcommands. */
static gpio_cmd_token_t gpio_cmd_handlers[] = {
    {(char *[]){"r", "read", 0}, gpio_cmd_read_handler},
    {(char *[]){"w", "write", 0}, gpio_cmd_write_handler},
    {(char *[]){"w", "blink", 0}, gpio_cmd_blink_handler},
    {0},
};

/**
 * Find a `gpio` subcommand given a token.
 *
 * \return the subcommand, or NULL if not found.
 */
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

/** `gpio` command handler function. */
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

/** `gpio` command description. */
const cmd_t gpio_command = {
    .name = "gpio",
    .description = "Control GPIO ports",
    .handler = gpio_cmd_handler,
};
