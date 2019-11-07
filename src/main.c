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

    if (!terminal_init(configMINIMAL_STACK_SIZE, tskIDLE_PRIORITY + 1)) {
        return 1;
    }

    cmd_t *echo_command = echo_init();
    cmd_t *gpio_command = gpio_init(configMINIMAL_STACK_SIZE, tskIDLE_PRIORITY + 4);
    cmd_t *i2c_command = i2c_init(configMINIMAL_STACK_SIZE, tskIDLE_PRIORITY + 3);

    echo_command->next = gpio_command;
    gpio_command->next = i2c_command;

    if (!cli_init(configMINIMAL_STACK_SIZE, tskIDLE_PRIORITY + 2, echo_command)) {
        return 1;
    }

    vTaskStartScheduler();

    return 0;
}
