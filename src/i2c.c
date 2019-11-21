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
        "  slave <device_address> write <tx_data...> [no]stop\r\n"
        "      Eg: i2c slave 50 write 00:00:de:ad:be:ef stop\r\n"
        "  slave <device_address> write <tx_data...> [no]stop read <rx_nbytes> [no]stop\r\n"
        "      Eg: i2c slave 50 write 00:00 stop read 4 stop\r\n"
        "  slave <device_address> read <rx_nbytes> [no]stop\r\n"
        "      Eg: i2c slave 50 read 4 stop\r\n"
    );
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
static void i2c_init(cmd_args_t *args) {
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

/**
 * Parse the `[no]stop` argument.
 *
 * \param s The input string.
 * \param out Output variable that will store the boolean stop condition
 *
 * \return false if the string cannot be parsed successfully.
 */
static bool parse_stop(const char *s, bool *out) {
    if (!strcmp(s, "stop")) {
        return *out = true;
        return true;
    }
    if (!strcmp(s, "nostop")) {
        return *out = false;
        return true;
    }
    return false;
}

/** Perform a write-read sequence on the slave device and then print the received data.  */
static void i2c_write_read_print(
    uint8_t device_address,
    size_t tx_nbytes,
    uint8_t tx_data[tx_nbytes],
    bool tx_stop,
    size_t rx_nbytes,
    bool rx_stop
) {
    uint8_t rx_data[rx_nbytes];

    if (!i2cRead(I2C0, device_address, tx_data, tx_nbytes, true, rx_data, rx_nbytes, true)) {
        log_error("Failed to read from i2c interface");
        return;
    }

    print_data(rx_data, rx_nbytes);
}

/** `i2c slave <xy> write <data> <[no]stop> read ...` command handler. */
static void i2c_write_read_cmd(uint8_t device_address, size_t tx_nbytes, uint8_t tx_data[tx_nbytes], bool tx_stop, cmd_args_t *args, unsigned int arg_read_index) {
    CMD_ASSERT_USAGE(args->count - arg_read_index == 3);
    CMD_ASSERT_USAGE(!strcmp(args->tokens[arg_read_index], "read"));

    int rx_nbytes = atoi(args->tokens[arg_read_index + 1]);
    CMD_ASSERT_USAGE(rx_nbytes > 0);

    bool rx_stop;
    CMD_ASSERT_USAGE(parse_stop(args->tokens[arg_read_index + 2], &rx_stop));

    i2c_write_read_print(device_address, tx_nbytes, tx_data, tx_stop, rx_nbytes, rx_stop);
}

/** `i2c slave <xy> write ...` command handler. */
static void i2c_write_cmd(uint8_t device_address, cmd_args_t *args) {
    CMD_ASSERT_USAGE(args->count >= 6);

    size_t tx_nbytes;
    CMD_ASSERT_USAGE(parse_data_nbytes(args->tokens[4], &tx_nbytes));

    uint8_t tx_data[tx_nbytes];
    CMD_ASSERT_USAGE(parse_data(args->tokens[4], tx_nbytes, tx_data));

    bool tx_stop;
    CMD_ASSERT_USAGE(parse_stop(args->tokens[5], &tx_stop));

    if (args->count > 6) {
        i2c_write_read_cmd(device_address, tx_nbytes, tx_data, tx_stop, args, 6);
        return;
    }

    if (!i2cWrite(I2C0, device_address, tx_data, tx_nbytes, true)) {
        log_error("Failed to write to i2c interface");
    }
}

/** `i2c slave <xy> read ...` command handler. */
static void i2c_read_cmd(uint8_t device_address, cmd_args_t *args) {
    uint8_t tx_data[1] = {0};
    i2c_write_read_cmd(device_address, 0, tx_data, true, args, 3);
}

/** `i2c slave` command handler. */
static void i2c_slave(cmd_args_t *args) {
    if (!i2c_freq_hz) {
        log_error("`i2c init` must be called first.");
        return;
    }
    CMD_ASSERT_USAGE(args->count >= 6);

    uint8_t device_address;
    CMD_ASSERT_USAGE(parse_device_address(args->tokens[2], &device_address));

    if (!strcmp(args->tokens[3], "write")) {
        i2c_write_cmd(device_address, args);
    } else if (!strcmp(args->tokens[3], "read")) {
        i2c_read_cmd(device_address, args);
    } else {
        CMD_ASSERT_USAGE(false);
    }
}

/** `i2c` command handler. */
static void i2c_cmd_handler(cmd_args_t *args) {
    CMD_ASSERT_USAGE(args->count >= 2);
    if (!strcmp(args->tokens[1], "help")) {
        usage();
    } else if (!strcmp(args->tokens[1], "init")) {
        i2c_init(args);
    } else if (!strcmp(args->tokens[1], "slave")) {
        i2c_slave(args);
    } else {
        CMD_ASSERT_USAGE(false);
    }
}

/** `i2c` command description. */
const cmd_t i2c_command = {
    .name = "i2c",
    .description = "Control the I2C interface",
    .handler = i2c_cmd_handler,
};
