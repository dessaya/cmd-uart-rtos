#include "FreeRTOS.h"
#include "task.h"
#include "sapi.h"
#include "terminal.h"
#include "cli.h"
#include "gpio.h"
#include "echo.h"
#include "i2c.h"

int main(void)
{
    boardInit();

    if (!terminal_init()) {
        return 1;
    }

    echo_command.next = &gpio_command;
    gpio_command.next = &i2c_command;

    if (!cli_init(&echo_command)) {
        return 1;
    }

    vTaskStartScheduler();

    return 0;
}
