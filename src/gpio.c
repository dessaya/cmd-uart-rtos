#include <string.h>
#include <assert.h>
#include "gpio.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "sapi.h"
#include "terminal.h"

/** Print the `gpio` command usage help. */
static void gpio_usage() {
    terminal_puts(
        "Usage: gpio <pin> <command> ...\r\n"
        "Examples:\r\n"
        "  gpio TEC1 read\r\n"
        "  gpio LEDB write 1\r\n"
        "  gpio LEDR toggle\r\n"
    );
}

/** GPIO port description and state. */
typedef struct {
    /** GPIO port name. */
    char *name;
    /** GPIO port pin number. */
    gpioMap_t pin;
    /** Mutex to synchronize concurrent access. */
    SemaphoreHandle_t mutex;
} gpio_port_t;

/** Utility macro to initialize the ports list. */
#define MAKE_PORT(gpio_port) { \
    .name = #gpio_port, \
    .pin = gpio_port, \
    .mutex = NULL, \
}

/** List of supported GPIO ports. */
static gpio_port_t ports[] = {
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
 * Find a gpio_port_t given its name.
 *
 * \return the port, or NULL if not found.
 */
static gpio_port_t *find_port(const char *name) {
    for (gpio_port_t *p = ports; p->name; p++) {
        if (!strcmp(p->name, name)) {
            return p;
        }
    }
    return NULL;
}

/** Take the mutex for the given port. */
static bool gpio_take_mutex(gpio_port_t *port, unsigned milliseconds) {
    if (xSemaphoreTake(port->mutex, pdMS_TO_TICKS(milliseconds)) == pdFALSE) {
        log_error("Failed to take mutex");
        return false;
    }
    return true;
}

/** Release the mutex for the given port. */
static bool gpio_release_mutex(gpio_port_t *port) {
    if (xSemaphoreGive(port->mutex) == pdFALSE) {
        log_error("Failed to release mutex");
        return false;
    }
    return true;
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
static void gpio_read_cmd_handler(gpio_port_t *port, cmd_args_t *args) {
    cli_assert(args->count == 3, gpio_usage);

    if (!gpio_take_mutex(port, 100)) {
        return;
    }
    bool_t pin_value = gpioRead(port->pin);
    gpio_release_mutex(port);

    terminal_println(on_off_to_string(pin_value));
}

/** `gpio <port> write` command handler function. */
static void gpio_write_cmd_handler(gpio_port_t *port, cmd_args_t *args) {
    cli_assert(args->count == 4, gpio_usage);

    bool_t on_off;
    cli_assert(parse_on_off_value(args->tokens[3], &on_off), gpio_usage);

    if (gpio_take_mutex(port, 100)) {
        gpioWrite(port->pin, on_off);
        gpio_release_mutex(port);
    }
}

/** `gpio <port> toggle` command handler function. */
static void gpio_toggle_cmd_handler(gpio_port_t *port, cmd_args_t *args) {
    cli_assert(args->count == 3, gpio_usage);

    if (gpio_take_mutex(port, 100)) {
        gpioToggle(port->pin);
        gpio_release_mutex(port);
    }
}

/** `gpio <port> <subcommand>` handler function interface. */
typedef void (*gpio_cmd_handler_t)(gpio_port_t *port, cmd_args_t *args);

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
    {(char *[]){"r", "read", 0}, gpio_read_cmd_handler},
    {(char *[]){"w", "write", 0}, gpio_write_cmd_handler},
    {(char *[]){"t", "toggle", 0}, gpio_toggle_cmd_handler},
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
    cli_assert(args->count >= 2, gpio_usage);
    if (!strcmp(args->tokens[1], "help")) {
        gpio_usage();
        return;
    }
    cli_assert(args->count >= 3, gpio_usage);
    gpio_port_t *port = find_port(args->tokens[1]);
    cli_assert(port, gpio_usage);
    gpio_cmd_handler_t command = find_gpio_cmd(args->tokens[2]);
    cli_assert(command, gpio_usage);

    if (port->mutex == NULL) {
        port->mutex = xSemaphoreCreateMutex();
        if (port->mutex == NULL) {
            log_error("Failed to create mutex");
            return;
        }
    }

    command(port, args);
}

/** `gpio` command description. */
const cmd_t gpio_command = {
    .name = "gpio",
    .description = "Control GPIO ports",
    .handler = gpio_cmd_handler,
};
