#include <string.h>
#include "cli.h"
#include "task.h"
#include "sapi.h"
#include "terminal.h"

static cmd_t *commands = NULL;

static void print_help() {
    terminal_println("Available commands:");
    for (const cmd_t *cmd = commands; cmd; cmd = cmd->next) {
        terminal_puts("  ");
        terminal_puts(cmd->cmd);
        terminal_puts(": ");
        terminal_println(cmd->description);
    }
}

static void help_cmd_handler() {
    print_help();
}

static cmd_t help_command = {
    .cmd = "help",
    .description = "List available commands",
    .handler = help_cmd_handler,
    .next = NULL,
};

static const cmd_t *find_command(const char *line) {
    for (const cmd_t *cmd = commands; cmd; cmd = cmd->next) {
        if (strcmp(cmd->cmd, line) == 0) {
            return cmd;
        }
    }
    return NULL;
}

static void cli_task(void *param) {
    commands = &help_command;
    help_command.next = (cmd_t *)param;

    terminal_println("");
    terminal_println("RTOS CLI initialized.");
    print_help(commands);

    while (1) {
        terminal_puts("$ ");

        char line[80];
        terminal_readline(line, sizeof(line));
        if (!*line) {
            continue;
        }

        const cmd_t *cmd = find_command(line);
        if (cmd) {
            cmd->handler();
        } else {
            terminal_puts("Unknown command: '");
            terminal_puts(line);
            terminal_println("'. Type 'help` to see a list of available commands.");
        }
    }
}

bool cli_init(configSTACK_DEPTH_TYPE stack_depth, UBaseType_t priority, cmd_t *commands) {
    if (!xTaskCreate(cli_task, "cliTask", stack_depth, commands, priority, 0)) {
        return 1;
    }

    return true;
}
