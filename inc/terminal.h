#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include "FreeRTOS.h"

bool terminal_init(configSTACK_DEPTH_TYPE stack_depth, UBaseType_t priority);

void terminal_putc(const char c);
void terminal_puts(const char *s);

char terminal_getc();
void terminal_gets(char *buf, size_t bufsize);

#endif
