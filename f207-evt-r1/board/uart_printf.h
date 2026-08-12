#ifndef __UART_PRINTF_H__
#define __UART_PRINTF_H__

void UART_Init(void);
int  UART_PutChar(int ch);
int  UART_GetChar(void);   /* blocking RX, 100 ms timeout, -1 on timeout */

#endif /* __UART_PRINTF_H__ */
