#include "i2c.h"
#include "terminal.h"

static struct {
    configSTACK_DEPTH_TYPE stack_depth;
    UBaseType_t priority;
} config = {0};

static void i2c_cmd_handler(cmd_args_t *args) {
    terminal_println(":)");
}

static cmd_t i2c_command = {
    .cmd = "i2c",
    .description = "Control the I2C port",
    .handler = i2c_cmd_handler,
    .next = NULL,
};

cmd_t *i2c_init(configSTACK_DEPTH_TYPE stack_depth, UBaseType_t priority) {
    config.stack_depth = stack_depth;
    config.priority = priority;

    return &i2c_command;
}
