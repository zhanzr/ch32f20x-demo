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
/* 1 ms SysTick tick (SysTick clock source = HCLK/8, as in the WCH EVT
 * debug.c). HAL_GetTick()/HAL_Delay() mirror the STM32 HAL API so benchmark
 * ports are the same shape as in the sibling stm32f1 repo. */

static volatile uint32_t uwTick = 0;

void SysTick_Handler(void)
{
    uwTick++;
}

void TICK_Init(void)
{
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
    SysTick->LOAD = (uint32_t)(SystemCoreClock / 8000U) - 1U;   /* 1 ms @ HCLK/8 */
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

uint32_t HAL_GetTick(void)
{
    return uwTick;
}

void HAL_Delay(uint32_t ms)
{
    uint32_t start = uwTick;
    while ((uint32_t)(uwTick - start) < ms)
    {
    }
}

/* ------------------------------------------------------------------------ */
/* Microsecond delay (SysTick polling; the tick counter is not used while
 * this runs). */

static uint8_t p_us = 0;

void Delay_Init(void)
{
    p_us = (uint8_t)(SystemCoreClock / 8000000U);
    TICK_Init();
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
    HAL_Delay(n);
}

/* ------------------------------------------------------------------------ */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
