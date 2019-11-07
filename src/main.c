#include "FreeRTOS.h"
#include "task.h"
#include "sapi.h"
#include "terminal.h"
#include "cli.h"
#include "gpio.h"

int main(void)
{
    boardInit();

    if (!terminal_init(configMINIMAL_STACK_SIZE, tskIDLE_PRIORITY + 1)) {
        return 1;
    }

    cmd_t *gpio_command = gpio_init(configMINIMAL_STACK_SIZE, tskIDLE_PRIORITY + 3);
    if (!gpio_command) {
        return 1;
    }

    if (!cli_init(configMINIMAL_STACK_SIZE, tskIDLE_PRIORITY + 2, gpio_command)) {
        return 1;
    }

    vTaskStartScheduler();

    return 0;
}
