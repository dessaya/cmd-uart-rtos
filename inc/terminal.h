#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdlib.h>
#include <stdbool.h>

bool terminal_init();

void terminal_putc(const char c);
void terminal_puts(const char *s);
void terminal_println(const char *s);

char terminal_getc();
void terminal_gets(char *buf, size_t bufsize);
void terminal_readline(char *buf, size_t bufsize);

#define log_error(msg) terminal_println("Error: " msg)

#endif
