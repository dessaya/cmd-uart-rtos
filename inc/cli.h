#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

#define CLI_ARGC_MAX 10

typedef struct {
    char *tokens[CLI_ARGC_MAX];
    int count;
} cmd_args_t;

typedef void (*cmd_handler_t)(cmd_args_t *args);

typedef struct cmd {
    char *cmd;
    char *description;
    cmd_handler_t handler;
    struct cmd *next;
} cmd_t;

bool cli_init(cmd_t *commands);

#endif

