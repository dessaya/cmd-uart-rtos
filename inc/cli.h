#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

/** Maximum length of a command line. */
#define CLI_LINE_MAX 80

/** Maximum amount of arguments for any given command. */
#define CLI_ARGC_MAX 20

/**
 * A parsed command, analogous to `argv` & `argc`.
 *
 * Example: if the user enters the command `gpio LED1 write 1`, this
 * struct will contain `tokens = {"gpio", "LED1", "write", "1"}`, and
 * `count = 4`.
 */
typedef struct {
    /** Command line buffer. Arguments are separated by null characters. */
    char buf[CLI_LINE_MAX];
    /** Pointers to the first character of each argument (`tokens[0]` would be the command name). */
    char *tokens[CLI_ARGC_MAX];
    /** Amount of arguments (including the command itself). */
    int count;
} cmd_args_t;

/** Command handler function prototype. */
typedef void (*cmd_handler_t)(const cmd_args_t *args);

/** Command definition. \see commands */
typedef struct cmd {
    /** Command name. */
    char *name;
    /** Command description, used for displaying help. */
    char *description;
    /** Command handler function. */
    cmd_handler_t handler;
} cmd_t;

/** Create the CLI task. */
bool cli_init();

/** `help` command definition. */
extern const cmd_t help_command;

/** Extract a subcommand starting from a given argument index. */
void cli_extract_subcommand(const cmd_args_t *cmd, unsigned subcmd_arg_index, cmd_args_t *subcmd);

/** Execute the command. */
void cli_exec_command(const cmd_args_t *args);

/**
 * Utility macro: check a condition; if it's false, print an error message,
 * execute the given commands and return.
 */
#define cli_assert(cond, usage) do { \
    if (!(cond)) { \
        terminal_println("Error: Invalid command."); \
        terminal_println(""); \
        usage(); \
        return; \
    } \
} while (0)

#endif

