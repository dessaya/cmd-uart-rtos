#include "gpio.h"
#include "terminal.h"

static void echo_cmd_handler(cmd_args_t *args) {
    for (int i = 1; i < args->count; i++) {
        terminal_puts(args->tokens[i]);
        if (i < args->count - 1)
            terminal_putc(' ');
    }
    terminal_putc('\r');
    terminal_putc('\n');
}

cmd_t echo_command = {
    .cmd = "echo",
    .description = "Print some text",
    .handler = echo_cmd_handler,
    .next = NULL,
};
