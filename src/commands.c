#include <stddef.h>
#include <string.h>
#include "commands.h"
#include "echo.h"
#include "sleep.h"
#include "loop.h"
#include "gpio.h"
#include "irq.h"
#include "i2c.h"

const cmd_t *commands[] = {
    &help_command,
    &echo_command,
    &sleep_command,
    &loop_command,
    &gpio_command,
    &irq_command,
    &i2c_command,
    0,
};

const cmd_t *find_command(const char *name) {
    for (const cmd_t **cmd = commands; *cmd; cmd++) {
        if (strcmp((*cmd)->name, name) == 0) {
            return *cmd;
        }
    }
    return NULL;
}
