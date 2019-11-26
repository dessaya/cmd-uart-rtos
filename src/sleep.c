#include "sleep.h"
#include "terminal.h"
#include "FreeRTOS.h"
#include "task.h"

/** Print the `sleep` command usage help. */
static void usage() {
    terminal_puts(
        "Usage: sleep <ms>\r\n"
        "   Eg: sleep 1000\r\n"
    );
}

/** `sleep` command handler function. */
static void sleep_cmd_handler(const cmd_args_t *args) {
    cli_assert(args->count == 2, usage);

    int32_t ms = atoi(args->tokens[1]);
    cli_assert(ms >= 0, usage);

    vTaskDelay(ms / portTICK_PERIOD_MS);
}

const cmd_t sleep_command = {
    .name = "sleep",
    .description = "Delay a given number of milliseconds",
    .handler = sleep_cmd_handler,
};
