#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include "FreeRTOS.h"

typedef void (*cmd_handler_t)();

typedef struct cmd {
    char *cmd;
    char *description;
    cmd_handler_t handler;
    struct cmd *next;
} cmd_t;

bool cli_init(configSTACK_DEPTH_TYPE stack_depth, UBaseType_t priority, cmd_t *commands);

#endif

