#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "i2c.h"
#include "terminal.h"
#include "sapi.h"

/** Maximum size of receive data buffer (bytes) */
#define RX_DATA_MAX 256
/** Maximum size of transmit data buffer (bytes) */
#define TX_DATA_MAX 256

/** Print the `i2c` command usage help. */
static void usage() {
    terminal_puts(
        "Usage: i2c <command> ...\r\n"
        "\r\n"
        "Commands:\r\n"
        "\r\n"
        "  help\r\n"
        "\r\n"
        "  init <freq>\r\n"
        "      Eg: i2c init 400000\r\n"
        "\r\n"
        "  slave <device_address> tx <tx_data...> [no]stop\r\n"
        "      Eg: i2c slave 50 tx 00:00:de:ad:be:ef stop\r\n"
        "\r\n"
        "  slave <device_address> tx <tx_data...> [no]stop rx <rx_nbytes> [no]stop\r\n"
        "      Eg: i2c slave 50 tx 00:00 stop rx 4 stop\r\n"
        "\r\n"
        "  slave <device_address> rx <rx_nbytes> [no]stop\r\n"
        "      Eg: i2c slave 50 rx 4 nostop\r\n"
    );
}

// https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html#Stringizing
#define str(a) #a
#define xstr(a) str(a)

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
    cli_assert(args->count >= 3, usage);
    int32_t freq = atoi(args->tokens[2]);
    cli_assert(freq >= 0 && freq <= 1000000, usage);
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

/**
 * Try to parse the `rx <rx_nbytes> [no]stop` section of command line.
 *
 * \param args The tokenized comand line arguments
 * \param token_index The current token index. All previous tokens have already been parsed.
 * \param rx_nbytes (out) The parsed amount of bytes to read.
 * \param rx_stop (out) The parsed stop condition.
 *
 * \return false if the rx section cannot be parsed successfully starting from token_index.
 */
static bool parse_read(const cmd_args_t *args, unsigned token_index, size_t *rx_nbytes, bool *rx_stop) {
    *rx_nbytes = atoi(args->tokens[token_index + 1]);
    if (*rx_nbytes >= RX_DATA_MAX) {
        log_error("Cannot receive more than " xstr(RX_DATA_MAX) " bytes");
        return false;
    }
    return parse_stop(args->tokens[token_index + 2], rx_stop);
}

/**
 * Try to parse the `tx <tx_data> [no]stop` section of command line.
 *
 * \param args The tokenized comand line arguments
 * \param token_index The current token index. All previous tokens have already been parsed.
 * \param tx_nbytes (out) The amount of bytes in the parsed data buffer.
 * \param tx_data (out) The data buffer that will store the parsed bytes.
 * \param tx_stop (out) The parsed stop condition.
 *
 * \return false if the tx section cannot be parsed successfully starting from token_index.
 */
static bool parse_write(const cmd_args_t *args, unsigned token_index, size_t *tx_nbytes, uint8_t tx_data[], bool *tx_stop) {
    if (!parse_data_nbytes(args->tokens[token_index + 1], tx_nbytes)) {
        return false;
    }
    if (*tx_nbytes >= TX_DATA_MAX) {
        log_error("Cannot tx more than " xstr(TX_DATA_MAX) " bytes");
        return false;
    }
    if (!parse_data(args->tokens[token_index + 1], *tx_nbytes, tx_data)) {
        return false;
    }
    return parse_stop(args->tokens[token_index + 2], tx_stop);
}

/** Perform a write-read sequence on the I2C slave device and then print the received data. */
static void i2c_write_read_print(uint8_t device_address, size_t tx_nbytes, uint8_t tx_data[], bool tx_stop, size_t rx_nbytes, bool rx_stop) {
    static uint8_t rx_data[RX_DATA_MAX];

    if (!i2cRead(I2C0, device_address, tx_data, tx_nbytes, true, rx_data, rx_nbytes, true)) {
        log_error("Failed to read from i2c device");
        return;
    }

    print_data(rx_data, rx_nbytes);
}

/** Transmit data to the I2C slave device. */
static void i2c_write(uint8_t device_address, size_t tx_nbytes, uint8_t tx_data[], bool stop) {
    if (!i2cWrite(I2C0, device_address, tx_data, tx_nbytes, stop)) {
        log_error("Failed to write to i2c device");
    }
}

/** `i2c slave ...` command handler. */
static void i2c_slave(cmd_args_t *args) {
    if (!i2c_freq_hz) {
        log_error("`i2c init` must be called first.");
        return;
    }
    cli_assert(args->count >= 6, usage);

    uint8_t device_address;
    cli_assert(parse_device_address(args->tokens[2], &device_address), usage);

    // arguments for `tx` section
    size_t tx_nbytes = 0;
    static uint8_t tx_data[TX_DATA_MAX];
    bool tx_stop = true;

    // arguments for `rx` section
    size_t rx_nbytes = 0;
    bool rx_stop = true;

    if (!strcmp(args->tokens[3], "rx") && args->count == 6) {
        // i2c slave <device_address> rx <rx_nbytes> [no]stop
        cli_assert(parse_read(args, 3, &rx_nbytes, &rx_stop), usage);
        i2c_write_read_print(device_address, tx_nbytes, tx_data, tx_stop, rx_nbytes, rx_stop);
        return;
    }

    if (!strcmp(args->tokens[3], "tx") && args->count == 6) {
        // i2c slave <device_address> tx <tx_data...> [no]stop
        cli_assert(parse_write(args, 3, &tx_nbytes, tx_data, &tx_stop), usage);
        i2c_write(device_address, tx_nbytes, tx_data, tx_stop);
        return;
    }

    if (!strcmp(args->tokens[3], "tx") && !strcmp(args->tokens[6], "rx") && args->count == 9) {
        // i2c slave <device_address> tx <tx_data...> [no]stop rx <rx_nbytes> [no]stop
        cli_assert(parse_write(args, 3, &tx_nbytes, tx_data, &tx_stop), usage);
        cli_assert(parse_read(args, 6, &rx_nbytes, &rx_stop), usage);
        i2c_write_read_print(device_address, tx_nbytes, tx_data, tx_stop, rx_nbytes, rx_stop);
        return;
    }

    cli_assert(false, usage);
}

/** `i2c` command handler. */
static void i2c_cmd_handler(cmd_args_t *args) {
    cli_assert(args->count >= 2, usage);
    if (!strcmp(args->tokens[1], "help")) {
        usage();
    } else if (!strcmp(args->tokens[1], "init")) {
        i2c_init(args);
    } else if (!strcmp(args->tokens[1], "slave")) {
        i2c_slave(args);
    } else {
        cli_assert(false, usage);
    }
}

const cmd_t i2c_command = {
    .name = "i2c",
    .description = "Control the I2C interface",
    .handler = i2c_cmd_handler,
};
