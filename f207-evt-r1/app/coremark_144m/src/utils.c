#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "board.h"
#include "utils.h"

#define BUF_SIZE 512

void uart_printf(const char *fmt, ...)
{
    char buf[BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s", buf);
}

/* HAL_GetTick(), HAL_Delay() and TICK_Init() are provided by the shared
 * board layer (board.c); nothing more to add here. */
