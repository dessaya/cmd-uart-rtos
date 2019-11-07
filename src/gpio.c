#include <string.h>
#include "gpio.h"
#include "terminal.h"
#include "sapi.h"

typedef enum {READ, WRITE} rw_t;

typedef struct {
    char *name;
    gpioMap_t port;
    rw_t rw;
    BaseType_t task;
} pin_t;

static pin_t pins[] = {
    {"TEC1", TEC1, READ, 0},
    {"TEC2", TEC2, READ, 0},
    {"TEC3", TEC3, READ, 0},
    {"TEC4", TEC4, READ, 0},

    {"LEDR", LEDR, WRITE, 0},
    {"LEDG", LEDG, WRITE, 0},
    {"LEDB", LEDB, WRITE, 0},
    {"LED1", LED1, WRITE, 0},
    {"LED2", LED2, WRITE, 0},
    {"LED3", LED3, WRITE, 0},
    {0},
};

static pin_t *find_pin(const char *name) {
    for (pin_t *p = pins; p->name; p++) {
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

char *value_to_string(bool_t value) {
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

typedef struct {
    char *name;
    rw_t rw;
} rw_token_t;

static rw_token_t rw_tokens[] = {
    {"r", READ},
    {"read", READ},
    {"w", WRITE},
    {"write", WRITE},
    {0},
};

static rw_token_t *find_rw_token(const char *name) {
    for (rw_token_t *t = rw_tokens; t->name; t++) {
        if (!strcmp(t->name, name)) {
            return t;
        }
    }
    return NULL;
}

static struct {
    configSTACK_DEPTH_TYPE stack_depth;
    UBaseType_t priority;
} config = {0};

static void usage() {
    terminal_println("Usage: gpio <pin> <command> ...");
    terminal_println("Examples:");
    terminal_println("  gpio TEC1 read");
    terminal_println("  gpio LEDB write 1");
}

#define CMD_ASSERT(cond) do { \
    if (!(cond)) { \
        usage(); \
        return; \
    } \
} while (0)

static void gpio_cmd_handler(cmd_args_t *args) {
    CMD_ASSERT(args->count >= 3);
    pin_t *pin = find_pin(args->tokens[1]);
    CMD_ASSERT(pin);
    rw_token_t *command = find_rw_token(args->tokens[2]);
    CMD_ASSERT(command);

    if (command->rw == READ) {
        terminal_println(value_to_string(gpioRead(pin->port)));
    } else {
        CMD_ASSERT(args->count >= 4);
        value_token_t *vt = find_value(args->tokens[3]);
        CMD_ASSERT(vt);

        gpioWrite(pin->port, vt->value);
    }
}

static cmd_t gpio_command = {
    .cmd = "gpio",
    .description = "Control GPIO ports",
    .handler = gpio_cmd_handler,
    .next = NULL,
};

cmd_t *gpio_init(configSTACK_DEPTH_TYPE stack_depth, UBaseType_t priority) {
    config.stack_depth = stack_depth;
    config.priority = priority;

    return &gpio_command;
}
