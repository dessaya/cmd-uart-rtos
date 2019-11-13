#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "i2c.h"
#include "terminal.h"
#include "sapi.h"

/** Print the `i2c` command usage help. */
static void usage() {
    terminal_puts(
        "Usage: i2c <command> ...\r\n"
        "Commands:\r\n"
        "  help\r\n"
        "  init <freq>\r\n"
        "      Eg: i2c init 400000\r\n"
        "  write <device_address> <tx_data...>\r\n"
        "      Eg: i2c write 50 00:00:de:ad:be:ef\r\n"
        "  read <device_address> <tx_data...> <rx_nbytes>\r\n"
        "      Eg: i2c read 50 00:00 4\r\n"
    );
}

/** `i2c help` command handler function. */
static void i2c_cmd_help_handler(cmd_args_t *args) {
    usage();
}

/** Utility macro: check a condition; if it's false, print the command usage help and return. */
#define CMD_ASSERT_USAGE(cond) do { \
    if (!(cond)) { \
        usage(); \
        return; \
    } \
} while (0)

/**
 * Global i2c configuration, controlled with `i2c init`.
 *
 * If 0, i2c is not configured. Otherwise, this variable contains the i2c bus frequency (in Hz).
 */
static unsigned int i2c_freq_hz = 0;

/**
 * `i2c init` command handler function.
 */
static void i2c_cmd_init_handler(cmd_args_t *args) {
    CMD_ASSERT_USAGE(args->count >= 3);
    int32_t freq = atoi(args->tokens[2]);
    CMD_ASSERT_USAGE(freq >= 0 && freq <= 1000000);
    i2c_freq_hz = freq;
    if (i2c_freq_hz > 0) {
        if (!i2cInit(I2C0, i2c_freq_hz)) {
            log_error("Failed to initialize i2c interface");
            return;
        }
    }
}

/**
 * Given a sequence of bytes expressed in hex format, this function returns the amount of
 * bytes needed to parse it.
 *
 * Eg: `parse_data_nbytes("aa:bb:cc:dd:ee", &out) sets `*out = 5`.
 *
 * \param data_hex The input string.
 * \param out Output variable that will store the detected amount of bytes.
 *
 * \return false if the data cannot be parsed successfully.
 */
static bool parse_data_nbytes(const char *data_hex, size_t *out) {
    if (strlen(data_hex) % 3 != 2) {
        return false;
    }
    *out = strlen(data_hex) / 3 + 1;
    return true;
}

/**
 * Parse a sequence of bytes expressed in hex format.
 *
 * \param data_hex The input string.
 * \param nbytes The amount of bytes expressed in data_hex. It should have been previously
 *               computed with parse_data_nbytes.
 * \param out A buffer containing space for nbytes bytes.
 *
 * Eg: `parse_data_nbytes("aa:bb:cc:dd:ee", 5, out) sets `out = {0xaa, 0xbb, 0xcc, 0xdd, 0xee}`.
 *
 * \return false if the data cannot be parsed successfully.
 */
static bool parse_data(const char *data_hex, uint8_t nbytes, uint8_t out[]) {
    for (int i = 0; i < nbytes; i++) {
        const char *s = &data_hex[3 * i];
        if (s[2] != ':' && s[2] != '\0') {
            return false;
        }
        const char hex_byte[] = {tolower(s[0]), tolower(s[1]), '\0'};

        errno = 0;
        out[i] = strtol(hex_byte, NULL, 16);
        if (errno) {
            return false;
        }
    }
    return true;
}

/**
 * Parse a 7-bit i2c device addres in hex format (eg: `"5a"`).
 *
 * \param s The input string.
 * \param out Output variable that will store the device address.
 *
 * \return false if the string cannot be parsed successfully.
 */
static bool parse_device_address(const char *s, uint8_t *out) {
    if (strlen(s) != 2) {
        return false;
    }
    errno = 0;
    *out = strtol(s, NULL, 16);
    return errno == 0;
}

/** `i2c write` command handler function. */
static void i2c_cmd_write_handler(cmd_args_t *args) {
    if (!i2c_freq_hz) {
        log_error("`i2c init` must be called first.");
        return;
    }
    CMD_ASSERT_USAGE(args->count >= 4);

    uint8_t device_address;
    CMD_ASSERT_USAGE(parse_device_address(args->tokens[2], &device_address));

    size_t tx_nbytes;
    CMD_ASSERT_USAGE(parse_data_nbytes(args->tokens[3], &tx_nbytes));

    uint8_t tx_data[tx_nbytes];
    CMD_ASSERT_USAGE(parse_data(args->tokens[3], tx_nbytes, tx_data));

    if (!i2cWrite(I2C0, device_address, tx_data, tx_nbytes, true)) {
        log_error("Failed to write to i2c interface");
        return;
    }
}

/** Print a sequence of bytes in hexadecimal format. */
static void print_data(uint8_t data[], size_t nbytes) {
    char s[4];
    for (int i = 0; i < nbytes; i++) {
        sprintf(s, "%02x:", data[i]);
        if (i == nbytes - 1) {
            s[2] = '\0';
        }
        terminal_puts(s);
    }
    terminal_puts("\r\n");
}

/** `i2c read` command handler. */
static void i2c_cmd_read_handler(cmd_args_t *args) {
    if (!i2c_freq_hz) {
        log_error("`i2c init` must be called first.");
        return;
    }
    CMD_ASSERT_USAGE(args->count >= 5);

    uint8_t device_address;
    CMD_ASSERT_USAGE(parse_device_address(args->tokens[2], &device_address));

    size_t tx_nbytes;
    CMD_ASSERT_USAGE(parse_data_nbytes(args->tokens[3], &tx_nbytes));

    uint8_t tx_data[tx_nbytes];
    CMD_ASSERT_USAGE(parse_data(args->tokens[3], tx_nbytes, tx_data));

    int rx_nbytes = atoi(args->tokens[4]);
    CMD_ASSERT_USAGE(rx_nbytes > 0);

    uint8_t rx_data[rx_nbytes];

    if (!i2cRead(I2C0, device_address, tx_data, tx_nbytes, true, rx_data, rx_nbytes, true)) {
        log_error("Failed to read from i2c interface");
        return;
    }

    print_data(rx_data, rx_nbytes);
}

/** `i2c <subcommand>` handler function prototype. */
typedef void (*i2c_cmd_handler_t)(cmd_args_t *args);

/** `i2c <subcommand>` description. */
typedef struct {
    /** Accepted tokens for `<subcommand>`. Eg: `i2c write` and `i2c w` are equivalent. */
    char **tokens;
    /** Subcommand handler function. */
    i2c_cmd_handler_t handler;
} i2c_cmd_token_t;

/** List of `i2c` subcommands. */
static i2c_cmd_token_t i2c_cmd_handlers[] = {
    {(char *[]){"help", 0}, i2c_cmd_help_handler},
    {(char *[]){"i", "init", 0}, i2c_cmd_init_handler},
    {(char *[]){"r", "read", 0}, i2c_cmd_read_handler},
    {(char *[]){"w", "write", 0}, i2c_cmd_write_handler},
    {0},
};

/**
 * Find an `i2c` subcommand given its name.
 *
 * \return the subcommand description, or NULL of not found.
 */
static i2c_cmd_handler_t find_i2c_cmd(const char *name) {
    for (i2c_cmd_token_t *s = i2c_cmd_handlers; s->tokens; s++) {
        for (char **token = s->tokens; *token; token++) {
            if (!strcmp(*token, name)) {
                return s->handler;
            }
        }
    }
    return NULL;
}

/** `i2c` command handler. */
static void i2c_cmd_handler(cmd_args_t *args) {
    CMD_ASSERT_USAGE(args->count >= 2);
    i2c_cmd_handler_t command = find_i2c_cmd(args->tokens[1]);
    CMD_ASSERT_USAGE(command);
    command(args);
}

/** `i2c` command description. */
const cmd_t i2c_command = {
    .name = "i2c",
    .description = "Control the I2C interface",
    .handler = i2c_cmd_handler,
};
