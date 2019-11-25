#include <string.h>
#include "irq.h"
#include "task_priorities.h"
#include "terminal.h"
#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"

void irq_usage() {
    terminal_puts(
        "Usage:\r\n"
        "  irq <channel> <trigger> <raising|falling> <command...>\r\n"
        "  irq <channel> disable\r\n"
        "Examples:\r\n"
        "  irq 0 TEC1 falling echo hello\r\n"
        "  irq 0 disable\r\n"
    );
}

/** GPIO port description and state. */
typedef struct {
    /** GPIO port name. */
    char *name;

    /** Port for interrupt handler. */
    uint8_t port;
    /** Pin for interrupt handler. */
    uint8_t pin;
} gpio_trigger_t;

/** List of supported GPIO ports. */
static gpio_trigger_t triggers[] = {
    {"TEC1", 0, 4},
    {"TEC2", 0, 8},
    {"TEC3", 0, 9},
    {"TEC4", 1, 9},
    {0},
};

/**
 * Find a gpio_trigger_t given its name.
 *
 * \return the trigger, or NULL if not found.
 */
static gpio_trigger_t *find_trigger(const char *name) {
    for (gpio_trigger_t *t = triggers; t->name; t++) {
        if (!strcmp(t->name, name)) {
            return t;
        }
    }
    return NULL;
}

/** GPIO edge trigger type. */
typedef enum { RAISING, FALLING } edge_t;

/** Disable the GPIO IRQ. */
static void disableGPIOIrq(uint8_t irq_channel)
{
    NVIC_ClearPendingIRQ(PIN_INT0_IRQn + irq_channel);
    NVIC_DisableIRQ(PIN_INT0_IRQn + irq_channel);
}

/** Enable the GPIO IRQ with the given trigger. */
static void enableGPIOIrq(uint8_t irq_channel, gpio_trigger_t *trigger, edge_t edge)
{
   Chip_SCU_GPIOIntPinSel(irq_channel, trigger->port, trigger->pin);
   Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(irq_channel));
   Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(irq_channel));

   if (edge == RAISING) {
      Chip_PININT_EnableIntHigh(LPC_GPIO_PIN_INT, PININTCH(irq_channel));
   } else {
      Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(irq_channel));
   }

   NVIC_ClearPendingIRQ(PIN_INT0_IRQn + irq_channel);
   NVIC_SetPriority(PIN_INT0_IRQn + irq_channel, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
   NVIC_EnableIRQ(PIN_INT0_IRQn + irq_channel);
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

/** Amount of supported IRQ channels */
#define IRQ_CHANNELS 4

/** State of an IRQ channel. */
typedef struct {
    /** IRQ channel number (same as index in settings array). */
    uint8_t irq_channel;
    /** RTOS task handle for irq_subcommand_task. */
    TaskHandle_t task_handle;
    /** Command to execute when IRQ is triggered. */
    cmd_args_t subcmd;
} irq_settings_t;

/** `irq` global state. */
static irq_settings_t settings[IRQ_CHANNELS] = {
    {.irq_channel = 0},
    {.irq_channel = 1},
    {.irq_channel = 2},
    {.irq_channel = 3},
};

/** ISR triggered from the configured GPIO port, that notifies the corresponding RTOS task. */
static void handle_irq(uint8_t irq_channel) {
    BaseType_t context_switch_needed = pdFALSE;

    if (settings[irq_channel].task_handle != NULL) {
        vTaskNotifyGiveFromISR(settings[irq_channel].task_handle, &context_switch_needed);
    }

    Chip_PININT_ClearRiseStates(LPC_GPIO_PIN_INT, PININTCH(irq_channel));
    Chip_PININT_ClearFallStates(LPC_GPIO_PIN_INT, PININTCH(irq_channel));
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(irq_channel));

    portYIELD_FROM_ISR(context_switch_needed);
}

void GPIO0_IRQHandler()
{
    handle_irq(0);
}

void GPIO1_IRQHandler()
{
    handle_irq(1);
}

void GPIO2_IRQHandler()
{
    handle_irq(2);
}

void GPIO3_IRQHandler()
{
    handle_irq(3);
}

/**
 * FreeRTOS task that waits for the configured GPIO port to trigger the interrupt, and then
 * execute the configured subcommand.
 */
static void irq_subcommand_task(void *param) {
    uint8_t irq_channel = ((irq_settings_t *)param)->irq_channel;
    while (1) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

        terminal_puts("GPIO triggered interrupt; executing `");
        terminal_puts(settings[irq_channel].subcmd.tokens[0]);
        terminal_println("` command.");

        cli_exec_command(&settings[irq_channel].subcmd);
    }
}

/** `irq` command handler function. */
static void irq_cmd_handler(cmd_args_t *args) {
    cli_assert(args->count >= 2, irq_usage);

    int irq_channel = atoi(args->tokens[1]);
    cli_assert(irq_channel >= 0 && irq_channel < IRQ_CHANNELS, irq_usage);

    if (args->count == 3 && !strcmp(args->tokens[2], "disable")) {
        if (settings[irq_channel].task_handle != NULL) {
            disableGPIOIrq(irq_channel);
            vTaskDelete(settings[irq_channel].task_handle);
            settings[irq_channel].task_handle = NULL;
        }
        return;
    }

    if (args->count >= 5) {
        // irq <channel> <trigger> <falling|raising> <command...>
        if (settings[irq_channel].task_handle != NULL) {
            log_error("Channel is currently active. Disable it first with `irq <channel> disable`.");
            return;
        }

        gpio_trigger_t *trigger = find_trigger(args->tokens[2]);
        cli_assert(trigger, irq_usage);

        edge_t edge;
        cli_assert(parse_edge(args->tokens[3], &edge), irq_usage);

        cli_extract_subcommand(args, 4, &settings[irq_channel].subcmd);

        if (xTaskCreate(
            irq_subcommand_task,
            "irq task",
            configMINIMAL_STACK_SIZE * 2,
            &settings[irq_channel],
            IRQ_TASK_PRIORITY,
            &settings[irq_channel].task_handle
        ) != pdPASS) {
            log_error("Failed to create task");
        }

        enableGPIOIrq(irq_channel, trigger, edge);
        return;
    }

    cli_assert(false, irq_usage);
}

const cmd_t irq_command = {
    .name = "irq",
    .description = "Control IRQ handlers",
    .handler = irq_cmd_handler,
};
