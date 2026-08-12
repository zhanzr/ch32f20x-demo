#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "utils.h"
#include "custom_def.h"
#include "core_portme.h"

int coremark_main(void);

int main(void)
{
    Board_Init();
    TICK_Init();

    const uint32_t cpu_hz = SystemCoreClock;

    while (1)
    {
        printf("\r\n--- CoreMark run on CH32F207VCT6 @ %lu Hz ---\r\n",
               (unsigned long)cpu_hz);
        coremark_main();
        printf("--- CoreMark complete. %lu Hz, %s ---\r\n",
               (unsigned long)cpu_hz, COMPILER_NAME);
        for (int i = 0; i < 10; i++)
        {
            HAL_Delay(1000);
        }
    }

    return 0;
}
