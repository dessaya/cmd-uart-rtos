#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include "FreeRTOS.h"

bool cli_init(configSTACK_DEPTH_TYPE stackDepth, UBaseType_t priority);

#endif

