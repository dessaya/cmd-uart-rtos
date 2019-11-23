#include "terminal.h"
#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "task_priorities.h"

#define UART_PORT UART_USB

#define RXQUEUE_CAPACITY 128
#define TXQUEUE_CAPACITY 128

/** Input character buffer. */
static QueueHandle_t rxQueue;
/** Output character buffer. */
static QueueHandle_t txQueue;

/**
 * ISR executed when a character is received on the UART, which is enqueued on rxQueue.
 */
static void uart_rx_isr(void *unused)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    char c = uartRxRead(UART_PORT);
    xQueueSendToBackFromISR(rxQueue, &c, &higher_priority_task_woken);

    portYIELD_FROM_ISR(higher_priority_task_woken)
}

/**
 * RTOS task that waits until there is a character waiting in txQueue and writes
 * it to the UART, in an infinite loop.
 */
static void terminal_tx_task(void *param) {
    while (1) {
        char c;
        if (xQueueReceive(txQueue, &c, portMAX_DELAY)) {
            uartWriteByte(UART_PORT, c);
        }
    }
}

void terminal_putc(const char c) {
    xQueueSendToBack(txQueue, &c, portMAX_DELAY);
}

void terminal_puts(const char *s) {
    for (; *s; s++) {
        terminal_putc(*s);
    }
}

void terminal_println(const char *s) {
    terminal_puts(s);
    terminal_putc('\r');
    terminal_putc('\n');
}

char terminal_getc() {
    char c;
    xQueueReceive(rxQueue, &c, portMAX_DELAY);
    return c;
}

void terminal_gets(char *buf, size_t bufsize) {
    if (bufsize == 0) {
        return;
    }
    char *s = buf;
    while (s < buf + bufsize - 1) {
        char c = terminal_getc();
        *s++ = c;
        if (c == '\n') {
            break;
        }
    }
    *s = '\0';
}

bool terminal_init() {
    rxQueue = xQueueCreate(RXQUEUE_CAPACITY, sizeof(char));
    if (rxQueue == NULL) {
        log_error("Failed to create rxQueue");
        return false;
    }

    txQueue = xQueueCreate(TXQUEUE_CAPACITY, sizeof(char));
    if (txQueue == NULL) {
        log_error("Failed to create txQueue");
        return false;
    }

    uartConfig(UART_PORT, 115200);
    uartCallbackSet(UART_PORT, UART_RECEIVE, uart_rx_isr, NULL);
    uartInterrupt(UART_PORT, true);

    if (xTaskCreate(
        terminal_tx_task,
        "terminal_tx_task",
        configMINIMAL_STACK_SIZE,
        0,
        TERMINAL_TASK_PRIORITY,
        0
    ) != pdPASS) {
        log_error("Failed to create task");
        return false;
    }

    return true;
}
