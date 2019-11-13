#include <string.h>
#include "cli.h"
#include "commands.h"
#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "terminal.h"
#include "task_priorities.h"

/** Show the list of available commands and their descriptions. */
static void print_help() {
    terminal_println("Available commands:");
    for (const cmd_t **cmd = commands; *cmd; cmd++) {
        terminal_puts("  ");
        terminal_puts((*cmd)->name);
        terminal_puts(": ");
        terminal_println((*cmd)->description);
    }
}

/** Handler function for the `help` command. */
static void help_cmd_handler(cmd_args_t *args) {
    print_help();
}

const cmd_t help_command = {
    .name = "help",
    .description = "List available commands",
    .handler = help_cmd_handler,
};

/** Parse a command sentence into a cmd_args_t struct. */
static void parse(char *line, cmd_args_t *out) {
    *out = (cmd_args_t){{0}, 0};
    for (
        char *token = strtok(line, " \t");
        token && out->count < CLI_ARGC_MAX - 1;
        token = strtok(NULL, " \t")
    ) {
        out->tokens[out->count++] = token;
    }
}

/** Remove `\r\n`. \return false if no newline characters were found. */
bool str_rstrip(char s[]) {
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            s[i] = '\0';
            return true;
        }
    }
    return false;
}

/**
 * RTOS task for the command line interface.
 *
 * This task shows the prompt, waits for input, parses the command
 * and executes its handler function, in an infinite loop.
 */
static void cli_task(void *param) {
    terminal_println("");
    terminal_println("RTOS CLI initialized.");
    print_help();

    while (1) {
        terminal_puts("$ ");

        #define LINE_LENGTH 80

        static char line[LINE_LENGTH];
        terminal_gets(line, sizeof(line));
        if (!str_rstrip(line)) { // remove \r\n
            log_error("Line is too long.");
            // discard the rest of the line
            do {
                terminal_gets(line, sizeof(line));
            } while (!str_rstrip(line));
        }

        static cmd_args_t args;
        parse(line, &args);
        if (args.count == 0) {
            continue;
        }
        if (args.count >= CLI_ARGC_MAX) {
            log_error("Too many arguments.");
            continue;
        }

        const cmd_t *cmd = find_command(args.tokens[0]);
        if (!cmd) {
            terminal_puts("Unknown command: '");
            terminal_puts(line);
            terminal_println("'. Type 'help' to see a list of available commands.");
            continue;
        }

        cmd->handler(&args);
    }
}

bool cli_init() {
    if (xTaskCreate(
        cli_task,
        "cliTask",
        configMINIMAL_STACK_SIZE * 2,
        0,
        CLI_TASK_PRIORITY,
        0
    ) != pdPASS) {
        log_error("Failed to create task");
        return false;
    }

    return true;
}
