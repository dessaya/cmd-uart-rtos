#include "gpio.h"
#include "terminal.h"

static void echo_cmd_handler() {
    terminal_println("echo");
}

static cmd_t echo_command = {
    .cmd = "echo",
    .description = "Print some text",
    .handler = echo_cmd_handler,
    .next = NULL,
};

cmd_t *echo_init() {
    return &echo_command;
}
