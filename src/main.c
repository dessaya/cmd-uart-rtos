#include "FreeRTOS.h"
#include "task.h"
#include "sapi.h"
#include "terminal.h"
#include "cli.h"

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
