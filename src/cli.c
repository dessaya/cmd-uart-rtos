#include <string.h>
#include "cli.h"
#include "task.h"
#include "sapi.h"
#include "terminal.h"

typedef void (*cmd_handler_t)();

typedef struct {
    char *cmd;
    cmd_handler_t handler;
    char *help;
} cmd_t;

void cmd_gpio();
void cmd_help();

cmd_t commands[] = {
    {"gpio", cmd_gpio, "Control GPIO ports"},
    {"help", cmd_help, "Print help"},
    {0},
};

void cmd_gpio() {
    terminal_println(":)");
}

void cmd_help() {
    terminal_println("Commands:");
    for (cmd_t *cmd = commands; cmd->cmd; cmd++) {
        terminal_puts("  ");
        terminal_puts(cmd->cmd);
        terminal_puts(": ");
        terminal_println(cmd->help);
    }
}

cmd_t *cmd_find(char *line, cmd_t commands[]) {
    for (cmd_t *cmd = commands; cmd->cmd; cmd++) {
        if (strcmp(cmd->cmd, line) == 0) {
            return cmd;
        }
    }
    return NULL;
}

void cmd_readline(cmd_t commands[]) {
    terminal_puts("$ ");
    char line[80];
    terminal_readline(line, sizeof(line));
    if (!*line) {
        return;
    }
    cmd_t *cmd = cmd_find(line, commands);
    if (cmd) {
        cmd->handler();
    } else {
        terminal_puts("Unknown command: ");
        terminal_println(line);
        cmd_help();
    }
}

static void cli_task(void *param) {
    while (1) {
        cmd_readline(commands);
    }
}

bool cli_init(configSTACK_DEPTH_TYPE stack_depth, UBaseType_t priority) {
    if (!xTaskCreate(cli_task, "cliTask", stack_depth, 0, priority, 0)) {
        return 1;
    }

    return true;
}
