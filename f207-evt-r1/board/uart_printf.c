/**
  * @file    uart_printf.c
  * @brief   USART1 printf transport for the f207-evt-r1 board.
  *
  * USART1 is on PA9 (TX) / PA10 (RX) and is wired to the WCH-Link SERIAL
  * (VCP) port of the on-board WCH-Link (COMxx). Output is 115200 8-N-1,
  * blocking (polled) so nothing is dropped.
  */

#include "uart_printf.h"
#include "ch32f20x.h"

/* ------------------------------------------------------------------------ */
void UART_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* PA9 = USART1_TX: push-pull alternate function. */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA10 = USART1_RX: input with pull-up (driven by the WCH-Link TXD). */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200U;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

/* ------------------------------------------------------------------------ */
int UART_PutChar(int ch)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
    {
    }
    USART_SendData(USART1, (uint8_t)ch);
    return ch;
}

/* ------------------------------------------------------------------------ */
int UART_GetChar(void)
{
    uint32_t tick = 0;

    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET)
    {
        if (++tick > 7200000U)   /* ~100 ms @ 144 MHz HCLK */
        {
            return -1;
        }
    }
    return (int)USART_ReceiveData(USART1);
}
