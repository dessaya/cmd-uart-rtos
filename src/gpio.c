#include "gpio.h"
#include "terminal.h"

static struct {
    configSTACK_DEPTH_TYPE stack_depth;
    UBaseType_t priority;
} config = {0};

static void gpio_cmd_handler() {
    terminal_println(":)");
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
