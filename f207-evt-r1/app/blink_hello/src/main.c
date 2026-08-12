#include <stdio.h>

#include "board.h"
#include "uart_printf.h"

/* On-board LED1 on the f207-evt-r1 is NOT wired to the MCU on the PCB; for
 * this demo it is connected to PA0 with an external wire. LOW active:
 * GPIO_WriteBit(..., Bit_RESET) = ON. */
#define LED1_PORT GPIOA
#define LED1_PIN  GPIO_Pin_0

static void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = LED1_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED1_PORT, &GPIO_InitStructure);

    GPIO_WriteBit(LED1_PORT, LED1_PIN, Bit_SET);   /* OFF (active low) */
}

static void LED_Toggle(void)
{
    if (GPIO_ReadOutputDataBit(LED1_PORT, LED1_PIN) == Bit_SET)
    {
        GPIO_WriteBit(LED1_PORT, LED1_PIN, Bit_RESET);   /* ON */
    }
    else
    {
        GPIO_WriteBit(LED1_PORT, LED1_PIN, Bit_SET);     /* OFF */
    }
}

int main(void)
{
    uint32_t ticks = 0;

    Board_Init();          /* SystemCoreClock update + USART1 console + delay */
    LED_Init();

    printf("\r\n=== blink_hello on CH32F207VCT6 @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);
    printf("LED1 (PA0, low active, external wire) toggling every 500 ms ...\r\n");

    while (1)
    {
        LED_Toggle();
        printf("[%lu] LED1 %s\r\n", (unsigned long)++ticks,
               (GPIO_ReadOutputDataBit(LED1_PORT, LED1_PIN) == Bit_RESET) ? "ON " : "OFF");
        Delay_Ms(500);
    }

    return 0;
}
