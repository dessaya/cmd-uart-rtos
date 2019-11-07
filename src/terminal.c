#include "terminal.h"
#include "task.h"
#include "sapi.h"
#include "queue.h"

#define UART_PORT UART_USB

#define RXQUEUE_CAPACITY 128
#define TXQUEUE_CAPACITY 128

static QueueHandle_t rxQueue;
static QueueHandle_t txQueue;

static void uart_rx_isr(void *unused)
{
   char c = uartRxRead(UART_PORT);
   xQueueSendToBackFromISR(rxQueue, &c, NULL);
}

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

void terminal_readline(char *buf, size_t bufsize) {
    if (bufsize == 0) {
        return;
    }
    terminal_gets(buf, bufsize);
    for (char *s = buf; *s; s++) {
        if (*s == '\n' || *s == '\r') {
            *s = '\0';
            break;
        }
    }
}

bool terminal_init(configSTACK_DEPTH_TYPE stack_depth, UBaseType_t priority) {
    rxQueue = xQueueCreate(RXQUEUE_CAPACITY, sizeof(char));
    if (rxQueue == NULL) {
        return false;
    }

    txQueue = xQueueCreate(TXQUEUE_CAPACITY, sizeof(char));
    if (txQueue == NULL) {
        return false;
    }

    uartConfig(UART_PORT, 115200);
    uartCallbackSet(UART_PORT, UART_RECEIVE, uart_rx_isr, NULL);
    uartInterrupt(UART_PORT, true);

    if (!xTaskCreate(terminal_tx_task, "terminal_tx_task", stack_depth, 0, priority, 0)) {
        return false;
    }

    return true;
}
