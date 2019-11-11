#include <string.h>
#include "cli.h"
#include "commands.h"
#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "terminal.h"
#include "task_priorities.h"

static void print_help() {
    terminal_println("Available commands:");
    for (const cmd_t *cmd = commands[0]; cmd; cmd++) {
        terminal_puts("  ");
        terminal_puts(cmd->name);
        terminal_puts(": ");
        terminal_println(cmd->description);
    }
}

static void help_cmd_handler(cmd_args_t *args) {
    print_help();
}

const cmd_t help_command = {
    .name = "help",
    .description = "List available commands",
    .handler = help_cmd_handler,
};

static const cmd_t *find_command(const char *line) {
    for (const cmd_t *cmd = commands[0]; cmd; cmd++) {
        if (strcmp(cmd->name, line) == 0) {
            return cmd;
        }
    }
    return NULL;
}

cmd_args_t *parse(char *line) {
    static cmd_args_t args;

    args = (cmd_args_t){{0}, 0};
    for (
        char *token = strtok(line, " \t");
        token && args.count < CLI_ARGC_MAX - 1;
        token = strtok(NULL, " \t")
    ) {
        args.tokens[args.count++] = token;
    }
    return &args;
}

static void cli_task(void *param) {
    terminal_println("");
    terminal_println("RTOS CLI initialized.");
    print_help(commands);

    while (1) {
        terminal_puts("$ ");

        static char line[80];
        terminal_readline(line, sizeof(line));

        cmd_args_t *args = parse(line);
        if (args->count == 0) {
            continue;
        }

        const cmd_t *cmd = find_command(args->tokens[0]);
        if (cmd) {
            cmd->handler(args);
        } else {
            terminal_puts("Unknown command: '");
            terminal_puts(line);
            terminal_println("'. Type 'help' to see a list of available commands.");
        }
    }
}

bool cli_init() {
    if (!xTaskCreate(
        cli_task,
        "cliTask",
        configMINIMAL_STACK_SIZE,
        commands,
        CLI_TASK_PRIORITY,
        0
    )) {
        return 1;
    }

    return true;
}
