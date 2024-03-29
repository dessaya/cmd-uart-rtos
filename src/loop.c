#include <stdio.h>
#include <string.h>
#include "loop.h"
#include "task_priorities.h"
#include "terminal.h"
#include "FreeRTOS.h"
#include "task.h"

void loop_usage() {
    terminal_puts(
        "Usage:\r\n"
        "  loop start <period_ms> <command...>\r\n"
        "  loop stop <handle>\r\n"
        "Example:\r\n"
        "  $ loop start 1000 gpio LED1 toggle\r\n"
        "  Loop handle: 0\r\n"
        "  $ loop stop 0\r\n"
    );
}

/** Amount of supported loop tasks */
#define LOOP_TASKS 4

/** Parameters for the RTOS loop task. */
typedef struct {
    /** Index number in the settings array. */
    uint8_t loop_handle;
    /** Loop period in ms. */
    unsigned period;
    /** RTOS task handle. */
    TaskHandle_t task_handle;
    /** RTOS task name. */
    TaskHandle_t task_name;
    /** Command to execute in the loop. */
    cmd_args_t subcmd;
} loop_task_param_t;

static loop_task_param_t settings[LOOP_TASKS] = {
    {.loop_handle = 0, .task_name = "loop0"},
    {.loop_handle = 1, .task_name = "loop1"},
    {.loop_handle = 2, .task_name = "loop2"},
    {.loop_handle = 3, .task_name = "loop3"},
};

/** Find a free slot for a new loop. */
bool find_free_loop_slot(uint8_t *loop_handle) {
    for (int i = 0; i < LOOP_TASKS; i++) {
        if (settings[i].task_handle == NULL) {
            *loop_handle = i;
            return true;
        }
    }
    return false;
}

/** RTOS task for a spawned loop. */
static void loop_task(void *param) {
    uint8_t loop_handle = ((loop_task_param_t *)param)->loop_handle;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(settings[loop_handle].period));
        cli_exec_command(&settings[loop_handle].subcmd);
    }
}

/** `loop stop <handle>` command handler. */
static void loop_stop_cmd_handler(const cmd_args_t *args) {
    uint8_t loop_handle = atoi(args->tokens[2]);
    cli_assert(loop_handle < LOOP_TASKS, loop_usage);

    if (settings[loop_handle].task_handle == NULL) {
        log_error("Loop task not started");
        return;
    }

    vTaskDelete(settings[loop_handle].task_handle);
    settings[loop_handle].task_handle = NULL;
}

/** `loop start <period> <command>` command handler. */
static void loop_start_cmd_handler(const cmd_args_t *args) {
    int period = atoi(args->tokens[2]);
    cli_assert(period > 0, loop_usage);

    uint8_t loop_handle;
    if (!find_free_loop_slot(&loop_handle)) {
        log_error("Too many loops. Use `loop stop` to free a slot.");
        return;
    }

    settings[loop_handle].period = period;
    cli_extract_subcommand(args, 3, &settings[loop_handle].subcmd);

    if (xTaskCreate(
        loop_task,
        settings[loop_handle].task_name,
        configMINIMAL_STACK_SIZE,
        &settings[loop_handle],
        LOOP_TASK_PRIORITY,
        &settings[loop_handle].task_handle
    ) != pdPASS) {
        log_error("Failed to create task");
    }

    terminal_puts("Loop handle: ");
    char s[4];
    snprintf(s, sizeof(s), "%d", loop_handle);
    terminal_println(s);
}

/** `loop` command handler function. */
static void loop_cmd_handler(const cmd_args_t *args) {
    cli_assert(args->count >= 2, loop_usage);
    if (args->count == 3 && !strcmp(args->tokens[1], "stop")) {
        loop_stop_cmd_handler(args);
        return;
    }
    if (args->count >= 4 && !strcmp(args->tokens[1], "start")) {
        loop_start_cmd_handler(args);
        return;
    }
    cli_assert(false, loop_usage);
}

const cmd_t loop_command = {
    .name = "loop",
    .description = "Spawn loop tasks",
    .handler = loop_cmd_handler,
};
