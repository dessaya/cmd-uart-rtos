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

/** Parse the command line tokens, replacing spaces with null characters in the buffer. */
static void parse_arguments(cmd_args_t *args) {
    args->count = 0;
    for (
        char *token = strtok(args->buf, " \t");
        token && args->count < CLI_ARGC_MAX - 1;
        token = strtok(NULL, " \t")
    ) {
        args->tokens[args->count++] = token;
    }
}

/** Remove `\r\n`. \return false if no newline characters were found. */
static bool str_rstrip(char s[]) {
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            s[i] = '\0';
            return true;
        }
    }
    return false;
}

void cli_extract_subcommand(const cmd_args_t *cmd, unsigned subcmd_arg_index, cmd_args_t *subcmd) {
    memcpy(subcmd->buf, cmd->buf, CLI_LINE_MAX);
    subcmd->count = cmd->count - subcmd_arg_index;
    for (int i = 0; i < subcmd->count; i++) {
        subcmd->tokens[i] = cmd->tokens[subcmd_arg_index + i];
    }
}

void cli_exec_command(cmd_args_t *args) {
    const cmd_t *cmd = find_command(args->tokens[0]);
    if (!cmd) {
        terminal_puts("Unknown command: '");
        terminal_puts(args->tokens[0]);
        terminal_println("'. Type 'help' to see a list of available commands.");
        return;
    }
    cmd->handler(args);
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

    static cmd_args_t args = {0};

    while (1) {
        terminal_puts("$ ");

        terminal_gets(args.buf, CLI_LINE_MAX);
        if (!str_rstrip(args.buf)) { // remove \r\n
            log_error("Line is too long.");

            // discard the rest of the line
            do {
                terminal_gets(args.buf, CLI_LINE_MAX);
            } while (!str_rstrip(args.buf));

            continue;
        }

        parse_arguments(&args);

        if (args.count == 0) {
            continue;
        }

        if (args.count >= CLI_ARGC_MAX) {
            log_error("Too many arguments.");
            continue;
        }

        cli_exec_command(&args);
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

const cmd_t help_command = {
    .name = "help",
    .description = "List available commands",
    .handler = help_cmd_handler,
};
