#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "gpio.h"
#include "terminal.h"
#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_priorities.h"

/** Print the `gpio` command usage help. */
static void usage() {
    terminal_puts(
        "Usage: gpio <pin> <command> ...\r\n"
        "Examples:\r\n"
        "  gpio TEC1 read\r\n"
        "  gpio TEC1 interrupt falling echo hello\r\n"
        "  gpio TEC1 interrupt disable\r\n"
        "  gpio LEDB write 1\r\n"
        "  gpio LED1 blink 2000\r\n"
    );
}

/** GPIO port description and state. */
typedef struct {
    /** GPIO port name. */
    char *name;
    /** GPIO port pin number. */
    gpioMap_t pin;
    /** Name of the RTOS blink task. */
    char *blink_task_name;

    /** RTOS blink task handle for this GPIO port, or 0 if no blink task is currently active. */
    TaskHandle_t blink_task;

    /** Port for interrupt handler. */
    uint8_t chip_port;
    /** Pin for interrupt handler. */
    uint8_t chip_pin;
} port_t;

/** Utility macro to initialize the ports list. */
#define MAKE_PORT(gpio_port, cport, cpin) { \
    .name = #gpio_port, \
    .pin = gpio_port, \
    .blink_task_name = #gpio_port " blink task", \
    .blink_task = 0, \
    .chip_port = cport, \
    .chip_pin = cpin, \
}

/** List of supported GPIO ports. */
static port_t ports[] = {
    MAKE_PORT(TEC1, 0, 4),
    MAKE_PORT(TEC2, 0, 8),
    MAKE_PORT(TEC3, 0, 9),
    MAKE_PORT(TEC4, 1, 9),

    MAKE_PORT(LEDR, 5, 0),
    MAKE_PORT(LEDG, 5, 1),
    MAKE_PORT(LEDB, 5, 2),
    MAKE_PORT(LED1, 0, 14),
    MAKE_PORT(LED2, 1, 11),
    MAKE_PORT(LED3, 1, 12),
    {0},
};

/**
 * Find a port_t given its name.
 *
 * \return the port, or NULL if not found.
 */
static port_t *find_port(const char *name) {
    for (port_t *p = ports; p->name; p++) {
        if (!strcmp(p->name, name)) {
            return p;
        }
    }
    return NULL;
}

/**
 * Describes the different ways the user can turn a GPIO port on or off.
 *
 * Eg: `gpio LED1 write 1` and `gpio LED1 write high` are equivalent.
 */
static const char *on_off_tokens[][4] = {
    [HIGH] = {"high", "on", "1", 0},
    [LOW] = {"low", "off", "0", 0},
};

/** Return a string representing a GPIO on/off value. */
static const char *on_off_to_string(bool_t value) {
    assert(value == HIGH || value == LOW);
    return on_off_tokens[value][0];
}

/**
 * Parse a GPIO on/off value.
 *
 * \return false if the name does not correspond to an on/off value.
 */
static bool parse_on_off_value(const char *name, bool_t *out) {
    for (bool_t on_off = LOW; on_off <= HIGH; on_off++) {
        for (const char **s = on_off_tokens[on_off]; *s; s++) {
            if (!strcmp(*s, name)) {
                *out = on_off;
                return true;
            }
        }
    }
    return false;
}

/** `gpio <port> read` command handler function. */
static void gpio_read_cmd_handler(port_t *port, cmd_args_t *args) {
    terminal_println(on_off_to_string(gpioRead(port->pin)));
}

/** `gpio <port> write` command handler function. */
static void gpio_write_cmd_handler(port_t *port, cmd_args_t *args) {
    cli_assert(args->count >= 4, usage);
    bool_t on_off;
    cli_assert(parse_on_off_value(args->tokens[3], &on_off), usage);
    gpioWrite(port->pin, on_off);
}

/** Parameters for the RTOS blink task. */
typedef struct {
    /** GPIO port to blink. */
    port_t *port;
    /** Blink period in ms. */
    unsigned period;
} blink_task_param_t;

/** RTOS task for controlling a blinking GPIO port. */
static void gpio_blink_task(void *param) {
    port_t *port = ((blink_task_param_t *)param)->port;
    int period = ((blink_task_param_t *)param)->period;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (true) {
        gpioToggle(port->pin);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(period / 2));
    }
}

/** `gpio <port> blink` command handler function. */
static void gpio_blink_cmd_handler(port_t *port, cmd_args_t *args) {
    cli_assert(args->count >= 4, usage);
    int period = atoi(args->tokens[3]);
    cli_assert(period >= 0, usage);

    if (port->blink_task) {
        vTaskDelete(port->blink_task);
        port->blink_task = 0;
    }
    if (period > 0) {
        static blink_task_param_t param;
        param.port = port;
        param.period = period;
        if (xTaskCreate(
            gpio_blink_task,
            port->blink_task_name,
            configMINIMAL_STACK_SIZE,
            &param,
            GPIO_BLINK_TASK_PRIORITY,
            &port->blink_task
        ) != pdPASS) {
            log_error("Failed to create task");
        }
    }
}

typedef enum { RAISING, FALLING } edge_t;

static void disableGPIOIrq(uint8_t irqChannel)
{
    NVIC_ClearPendingIRQ(PIN_INT0_IRQn + irqChannel);
    NVIC_DisableIRQ(PIN_INT0_IRQn + irqChannel);
}

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

#define IRQ_CHANNEL 0
#define IRQ_HANDLER_NAME GPIO ## IRQ_CHANNEL ## _IRQHandler

static struct {
    TaskHandle_t task_handle;
    cmd_args_t subcmd;
} interrupt_settings = {0};

void gpio_subcommand_task(void *param) {
    while (1) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        cli_exec_command(&interrupt_settings.subcmd);
    }
}

/** `gpio <port> interrupt` command handler function. */
static void gpio_interrupt_cmd_handler(port_t *port, cmd_args_t *args) {
    if (args->count == 4 && !strcmp(args->tokens[3], "disable")) {
        if (interrupt_settings.task_handle == NULL) {
            log_error("No interrupt has been configured.");
            return;
        }
        // gpio <port> interrupt disable
        disableGPIOIrq(IRQ_CHANNEL);
        vTaskDelete(interrupt_settings.task_handle);
        interrupt_settings.task_handle = NULL;
        return;
    }

    if (args->count >= 5) {
        // gpio <port> interrupt <falling/raising> <command...>
        if (interrupt_settings.task_handle != NULL) {
            log_error("An interrupt has already been configured. Disable it first with `interrupt disable`.");
            return;
        }

        edge_t edge;
        cli_assert(parse_edge(args->tokens[3], &edge), usage);

        cli_extract_subcommand(args, 4, &interrupt_settings.subcmd);

        if (xTaskCreate(
            gpio_subcommand_task,
            "gpio interrupt wait task",
            configMINIMAL_STACK_SIZE * 2,
            &interrupt_settings.subcmd,
            GPIO_SUBCOMMAND_TASK_PRIORITY,
            &interrupt_settings.task_handle
        ) != pdPASS) {
            log_error("Failed to create task");
        }

        enableGPIOIrq(IRQ_CHANNEL, port->chip_port, port->chip_pin, edge);
        return;
    }

    cli_assert(false, usage);
}

void IRQ_HANDLER_NAME()
{
    BaseType_t context_switch_needed = pdFALSE;

    if (interrupt_settings.task_handle != NULL) {
        vTaskNotifyGiveFromISR(interrupt_settings.task_handle, &context_switch_needed);
    }

    Chip_PININT_ClearRiseStates(LPC_GPIO_PIN_INT, PININTCH(IRQ_CHANNEL));
    Chip_PININT_ClearFallStates(LPC_GPIO_PIN_INT, PININTCH(IRQ_CHANNEL));
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(IRQ_CHANNEL));

    portYIELD_FROM_ISR(context_switch_needed);
}

/** `gpio <port> <subcommand>` handler function interface. */
typedef void (*gpio_cmd_handler_t)(port_t *port, cmd_args_t *args);

/** `gpio <port> <subcommand>` definition. */
typedef struct {
    /**
     * Accepted tokens for `<subcommand>`.
     *
     * Eg: this allows the user to call `gpio LED1 read` or simply `gpio LED1 r`.
     */
    char **tokens;

    /** Subcommand handler function. */
    gpio_cmd_handler_t handler;
} gpio_cmd_token_t;

/** List of `gpio` subcommands. */
static gpio_cmd_token_t gpio_cmd_handlers[] = {
    {(char *[]){"r", "read", 0}, gpio_read_cmd_handler},
    {(char *[]){"w", "write", 0}, gpio_write_cmd_handler},
    {(char *[]){"b", "blink", 0}, gpio_blink_cmd_handler},
    {(char *[]){"i", "interrupt", 0}, gpio_interrupt_cmd_handler},
    {0},
};

/**
 * Find a `gpio` subcommand given a token.
 *
 * \return the subcommand, or NULL if not found.
 */
static gpio_cmd_handler_t find_gpio_cmd(const char *name) {
    for (gpio_cmd_token_t *s = gpio_cmd_handlers; s->tokens; s++) {
        for (char **token = s->tokens; *token; token++) {
            if (!strcmp(*token, name)) {
                return s->handler;
            }
        }
    }
    return NULL;
}

/** `gpio` command handler function. */
static void gpio_cmd_handler(cmd_args_t *args) {
    cli_assert(args->count >= 2, usage);
    if (!strcmp(args->tokens[1], "help")) {
        usage();
        return;
    }
    cli_assert(args->count >= 3, usage);
    port_t *port = find_port(args->tokens[1]);
    cli_assert(port, usage);
    gpio_cmd_handler_t command = find_gpio_cmd(args->tokens[2]);
    cli_assert(command, usage);
    command(port, args);
}

/** `gpio` command description. */
const cmd_t gpio_command = {
    .name = "gpio",
    .description = "Control GPIO ports",
    .handler = gpio_cmd_handler,
};
