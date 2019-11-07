#include "FreeRTOS.h"
#include "task.h"
#include "sapi.h"
#include "terminal.h"
#include "cli.h"

int main(void)
{
    boardInit();

    if (!terminal_init(configMINIMAL_STACK_SIZE, tskIDLE_PRIORITY + 1)) {
        return 1;
    }

    if (!cli_init(configMINIMAL_STACK_SIZE, tskIDLE_PRIORITY + 2)) {
        return 1;
    }

    vTaskStartScheduler();
}
