#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdlib.h>
#include <stdbool.h>

/** Initialize the RTOS task, interrupt and queues for controlling the terminal I/O. */
bool terminal_init();

/** Write a character to the terminal. */
void terminal_putc(const char c);
/** Write a string to the terminal. */
void terminal_puts(const char s[]);
/** Write a string to the terminal, appending `'\\r\\n'`. */
void terminal_println(const char s[]);

/** Read a single character from the terminal. */
char terminal_getc();
/** Read at most bufsize bytes from the terminal, or until a newline (included in the returned buffer). */
void terminal_gets(char buf[], size_t bufsize);

/** Print an error message. */
#define log_error(msg) terminal_println("Error: " msg)

#endif
