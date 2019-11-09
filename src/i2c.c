#include "i2c.h"
#include "terminal.h"

static void i2c_cmd_handler(cmd_args_t *args) {
    terminal_println(":)");
}

cmd_t i2c_command = {
    .cmd = "i2c",
    .description = "Control the I2C port",
    .handler = i2c_cmd_handler,
    .next = NULL,
};
