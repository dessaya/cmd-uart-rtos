#include "cli.h"
#include "task.h"
#include "sapi.h"
#include "terminal.h"

static void cli_task(void *param) {
    char lineBuffer[128];

    while (1) {
        terminal_puts("$ ");
        terminal_gets(lineBuffer, 128);
        terminal_puts(">> ");
        terminal_puts(lineBuffer);
    }
}

bool cli_init(configSTACK_DEPTH_TYPE stack_depth, UBaseType_t priority) {
    if (!xTaskCreate(cli_task, "cliTask", stack_depth, 0, priority, 0)) {
        return 1;
    }

    return true;
}
