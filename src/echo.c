#include "echo.h"
#include "terminal.h"

/** `echo` command handler function. */
static void echo_cmd_handler(const cmd_args_t *args) {
    for (int i = 1; i < args->count; i++) {
        terminal_puts(args->tokens[i]);
        if (i < args->count - 1)
            terminal_putc(' ');
    }
    terminal_putc('\r');
    terminal_putc('\n');
}

const cmd_t echo_command = {
    .name = "echo",
    .description = "Print some text",
    .handler = echo_cmd_handler,
};
