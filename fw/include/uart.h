#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>

/* Global flag indicating if USART2 on SAO connector is enabled */
extern bool uart2_enabled;

/* UART initialization */
void UART_Init(void);
void UART2_Init(void);  /* Initialize USART2 on SAO connector (PA3=RX, PA4=TX) */

/* UART transmit functions */
void UART_SendChar(char c);
void UART_SendString(const char *str);
void UART_SendData(const uint8_t *data, uint32_t len);

/* UART receive functions */
int UART_ReceiveChar(char *c);  /* Non-blocking: returns 1 if char available, 0 otherwise */
uint32_t UART_Available(void);   /* Returns number of bytes available */

/* UART IRQ handler (implemented in uart.c) */
void UART_IRQHandler(void);

#endif /* UART_H */
