#ifndef __BOARD_H__
#define __BOARD_H__

#include "ch32f20x.h"

/* Init: 144 MHz clocks (set up by SystemInit() in system_ch32f20x.c, called
 * from the startup file) + USART1 console (PA9/PA10, 115200 8-N-1) + SysTick
 * 1 ms tick + polling microsecond delays. */
void Board_Init(void);
void SystemClock_Config(void);  /* update SystemCoreClock for the PLL clock tree */
void TICK_Init(void);           /* 1 ms SysTick interrupt tick (HCLK/8 source) */
uint32_t HAL_GetTick(void);     /* millisecond counter, wraps after ~49 days */
void HAL_Delay(uint32_t ms);
void Delay_Init(void);
void Delay_Us(uint32_t n);      /* polling, disables the SysTick during use */
void Delay_Ms(uint16_t n);      /* == HAL_Delay(n) */
void Error_Handler(void);

#endif /* __BOARD_H__ */
