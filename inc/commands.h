#ifndef COMMANDS_H
#define COMMANDS_H

#include "cli.h"

/**
 * List of supported commands.
 */
extern const cmd_t *commands[];

/**
 * Find a command by its name.
 *
 * \return the command definition, or `NULL` if not found.
 */
const cmd_t *find_command(const char *name);

#endif

