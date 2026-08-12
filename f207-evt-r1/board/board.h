#ifndef __BOARD_H__
#define __BOARD_H__

#include "ch32f20x.h"

/* Init: 144 MHz clocks (set up by SystemInit() in system_ch32f20x.c, called
 * from the startup file) + USART1 console (PA9/PA10, 115200 8-N-1) + SysTick
 * polling delays. */
void Board_Init(void);
void SystemClock_Config(void);  /* update SystemCoreClock for the PLL clock tree */
void Delay_Init(void);
void Delay_Us(uint32_t n);
void Delay_Ms(uint16_t n);
void Error_Handler(void);

#endif /* __BOARD_H__ */
