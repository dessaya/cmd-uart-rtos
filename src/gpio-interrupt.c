#include <string.h>
#include "gpio-interrupt.h"
#include "task_priorities.h"
#include "terminal.h"

/** GPIO edge trigger type. */
typedef enum { RAISING, FALLING } edge_t;

/** Disable the GPIO IRQ. */
static void disableGPIOIrq(uint8_t irqChannel)
{
    NVIC_ClearPendingIRQ(PIN_INT0_IRQn + irqChannel);
    NVIC_DisableIRQ(PIN_INT0_IRQn + irqChannel);
}

/** Enable the GPIO IRQ with the given port and edge. */
static void enableGPIOIrq(uint8_t irqChannel, uint8_t port, uint8_t pin, edge_t edge)
{
   Chip_SCU_GPIOIntPinSel(irqChannel , port, pin);
   Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(irqChannel));
   Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(irqChannel));

   if (edge == RAISING) {
      Chip_PININT_EnableIntHigh(LPC_GPIO_PIN_INT, PININTCH(irqChannel));
   } else {
      Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(irqChannel));
   }

   NVIC_ClearPendingIRQ(PIN_INT0_IRQn + irqChannel);
   NVIC_EnableIRQ(PIN_INT0_IRQn + irqChannel);
}

/** Parse the edge trigger token. */
static bool parse_edge(const char *s, edge_t *out) {
    if (!strcmp(s, "falling")) {
        *out = FALLING;
        return true;
    }
    if (!strcmp(s, "raising")) {
        *out = RAISING;
        return true;
    }
    return false;
}

/** IRQ channel to use for the GPIO interrupt. */
#define IRQ_CHANNEL 0

/** `gpio interrupt` global state. */
static struct {
    TaskHandle_t task_handle;
    cmd_args_t subcmd;
} settings = {0};

/**
 * FreeRTOS task that waits for the configured GPIO port to trigger the interrupt, and then
 * execute the configured subcommand.
 */
static void gpio_subcommand_task(void *param) {
    while (1) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        cli_exec_command(&settings.subcmd);
    }
}

#define IRQ_HANDLER_NAME GPIO ## IRQ_CHANNEL ## _IRQHandler

/**
 * ISR triggered from the configured GPIO port, that notifies the GPIO subcommand task.
 */
void IRQ_HANDLER_NAME()
{
    BaseType_t context_switch_needed = pdFALSE;

    if (settings.task_handle != NULL) {
        vTaskNotifyGiveFromISR(settings.task_handle, &context_switch_needed);
    }

    Chip_PININT_ClearRiseStates(LPC_GPIO_PIN_INT, PININTCH(IRQ_CHANNEL));
    Chip_PININT_ClearFallStates(LPC_GPIO_PIN_INT, PININTCH(IRQ_CHANNEL));
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(IRQ_CHANNEL));

    portYIELD_FROM_ISR(context_switch_needed);
}

/** `gpio <port> interrupt` command handler function. */
void gpio_interrupt_cmd_handler(gpio_port_t *port, cmd_args_t *args) {
    if (args->count == 4 && !strcmp(args->tokens[3], "disable")) {
        if (settings.task_handle == NULL) {
            log_error("No interrupt has been configured.");
            return;
        }
        // gpio <port> interrupt disable
        disableGPIOIrq(IRQ_CHANNEL);
        vTaskDelete(settings.task_handle);
        settings.task_handle = NULL;
        return;
    }

    if (args->count >= 5) {
        // gpio <port> interrupt <falling/raising> <command...>
        if (settings.task_handle != NULL) {
            log_error("An interrupt has already been configured. Disable it first with `interrupt disable`.");
            return;
        }

        edge_t edge;
        cli_assert(parse_edge(args->tokens[3], &edge), gpio_usage);

        cli_extract_subcommand(args, 4, &settings.subcmd);

        if (xTaskCreate(
            gpio_subcommand_task,
            "gpio interrupt wait task",
            configMINIMAL_STACK_SIZE * 2,
            &settings.subcmd,
            GPIO_SUBCOMMAND_TASK_PRIORITY,
            &settings.task_handle
        ) != pdPASS) {
            log_error("Failed to create task");
        }

        enableGPIOIrq(IRQ_CHANNEL, port->chip_port, port->chip_pin, edge);
        return;
    }

    cli_assert(false, gpio_usage);
}
