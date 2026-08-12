/**
  * @file    board.c
  * @brief   Board init for the f207-evt-r1 (CH32F207VCT6, WCH CH32F20x EVT-R1).
  *
  * The 144 MHz clock tree is set up by SystemInit() in system_ch32f20x.c
  * (called from the startup file before main), using the PLL from the 8 MHz
  * HSE:
  *   HSE 8 MHz -> PLL (x18) -> SYSCLK 144 MHz
  *   AHB  /1   -> HCLK 144 MHz
  *   APB1 /2   -> PCLK1 72 MHz
  *   APB2 /1   -> PCLK2 144 MHz
  *
  * Console: USART1 (PA9 TX / PA10 RX) @ 115200 8-N-1 -> WCH-Link SERIAL.
  */

#include "board.h"
#include "uart_printf.h"

/* ------------------------------------------------------------------------ */
/* The PLL setup is already performed by SystemInit(); this only refreshes
 * SystemCoreClock from the actual RCC registers. */
void SystemClock_Config(void)
{
    SystemCoreClockUpdate();
}

/* ------------------------------------------------------------------------ */
void Board_Init(void)
{
    SystemClock_Config();
    UART_Init();
    Delay_Init();
}

/* ------------------------------------------------------------------------ */
/* SysTick-based polling delays (SysTick clock = HCLK/8, as in the WCH EVT
 * debug.c Delay_Init/Delay_Ms/Delay_Us). */

static uint8_t  p_us = 0;
static uint16_t p_ms = 0;

void Delay_Init(void)
{
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
    p_us = (uint8_t)(SystemCoreClock / 8000000U);
    p_ms = (uint16_t)((uint32_t)p_us * 1000U);
}

void Delay_Us(uint32_t n)
{
    uint32_t i;

    SysTick->LOAD = n * p_us;
    SysTick->VAL  = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

    do
    {
        i = SysTick->CTRL;
    } while ((i & 0x01) && !(i & (1U << 16)));

    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0x00;
}

void Delay_Ms(uint16_t n)
{
    uint32_t i;

    SysTick->LOAD = (uint32_t)n * p_ms;
    SysTick->VAL  = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

    do
    {
        i = SysTick->CTRL;
    } while ((i & 0x01) && !(i & (1U << 16)));

    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0x00;
}

/* ------------------------------------------------------------------------ */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
