#include "FreeRTOS.h"
#include "task.h"
#include "sapi.h"
#include "terminal.h"
#include "cli.h"
#include "commands.h"
#include "gpio.h"
#include "echo.h"
#include "i2c.h"

const cmd_t *commands[] = {
    &help_command,
    &echo_command,
    &gpio_command,
    &i2c_command,
    0,
};

int main(void)
{
    boardInit();

    if (!terminal_init()) {
        return 1;
    }

    if (!cli_init()) {
        return 1;
    }

    vTaskStartScheduler();

    return 0;
}
