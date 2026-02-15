/* UART driver with IRQ-driven TX/RX and ring buffers */

#include "uart.h"
#include "gpio.h"
#include "config.h"
#include "pins.h"
#include "system.h"

#include "stm32c011xx.h"
#include <stdbool.h>

/* Global flag to indicate if USART2 on SAO connector is enabled */
bool uart2_enabled = false;

/* Simple blocking transmit using USART2 TDR/ISR flags */
void UART_Init(void) {
    /* Enable GPIO clocks for TX/RX pins */
    GPIO_ClockEnable(UART_TX_GPIO_PORT);
    GPIO_ClockEnable(UART_RX_GPIO_PORT);

    /* Enable USART1 clock (APB peripheral enable) - board uses USART1 (PA4/PA3)
     * On STM32C0 USART1 clock enable is in APBENR2 (RCC_APBENR2_USART1EN)
     */
    RCC->APBENR2 |= RCC_APBENR2_USART1EN;

    /* Configure GPIO pins for UART alternate function */
    GPIO_SetMode(UART_TX_GPIO_PORT, UART_TX_GPIO_PIN, GPIO_MODE_AF);
    GPIO_SetOutputType(UART_TX_GPIO_PORT, UART_TX_GPIO_PIN, GPIO_OTYPE_PP);
    GPIO_SetSpeed(UART_TX_GPIO_PORT, UART_TX_GPIO_PIN, GPIO_SPEED_HIGH);
    GPIO_SetPullUpDown(UART_TX_GPIO_PORT, UART_TX_GPIO_PIN, GPIO_PUPD_PU);
    GPIO_SetAlternateFunction(UART_TX_GPIO_PORT, UART_TX_GPIO_PIN, UART_TX_AF);

    GPIO_SetMode(UART_RX_GPIO_PORT, UART_RX_GPIO_PIN, GPIO_MODE_AF);
    GPIO_SetSpeed(UART_RX_GPIO_PORT, UART_RX_GPIO_PIN, GPIO_SPEED_HIGH);
    GPIO_SetPullUpDown(UART_RX_GPIO_PORT, UART_RX_GPIO_PIN, GPIO_PUPD_PU);
    GPIO_SetAlternateFunction(UART_RX_GPIO_PORT, UART_RX_GPIO_PIN, UART_RX_AF);

    /* Basic USART configuration: baudrate, 8N1 */
    UART_PERIPHERAL->CR1 = 0;
    UART_PERIPHERAL->CR2 = 0;
    UART_PERIPHERAL->CR3 = 0;

    uint32_t pclk = System_GetClock();
    UART_PERIPHERAL->BRR = pclk / UART_BAUDRATE;

    /* Enable transmitter and receiver */
    UART_PERIPHERAL->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    /* Optionally enable RX interrupt (RXNE/RXFIFO not empty) */
    UART_PERIPHERAL->CR1 |= USART_CR1_RXNEIE_RXFNEIE;
    NVIC_EnableIRQ(UART_IRQn);
}

/* Blocking send (wait for TXE and write) */
void UART_SendChar(char c) {
    while (!(UART_PERIPHERAL->ISR & USART_ISR_TXE_TXFNF));
    UART_PERIPHERAL->TDR = (uint8_t)c;
    
    /* Also send on USART2 if enabled */
    if (uart2_enabled) {
        while (!(USART2->ISR & USART_ISR_TXE_TXFNF));
        USART2->TDR = (uint8_t)c;
    }
    /* Wait for TC if strict completion needed */
}

void UART_SendString(const char *str) {
    while (*str) UART_SendChar(*str++);
}

void UART_SendData(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) UART_SendChar(data[i]);
}

/* RX buffer for IRQ-driven receive */
static volatile uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint32_t uart_rx_head = 0;
static volatile uint32_t uart_rx_tail = 0;

int UART_ReceiveChar(char *c) {
    if (uart_rx_head == uart_rx_tail) return 0;
    *c = uart_rx_buffer[uart_rx_tail];
    uart_rx_tail = (uart_rx_tail + 1) % UART_RX_BUFFER_SIZE;
    return 1;
}

uint32_t UART_Available(void) {
    return (uart_rx_head - uart_rx_tail + UART_RX_BUFFER_SIZE) % UART_RX_BUFFER_SIZE;
}

void UART_IRQHandler(void) {
    if (UART_PERIPHERAL->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t d = UART_PERIPHERAL->RDR;
        uint32_t next = (uart_rx_head + 1) % UART_RX_BUFFER_SIZE;
        if (next != uart_rx_tail) {
            uart_rx_buffer[uart_rx_head] = d;
            uart_rx_head = next;
        }
    }
}

/* IRQ wrapper: startup vectors expect USART1_IRQHandler for this board */
void USART1_IRQHandler(void) {
    UART_IRQHandler();
}

/* Initialize USART2 on SAO connector (PA3=RX, PA4=TX) */
void UART2_Init(void) {
    /* Enable GPIO clocks for SAO UART TX/RX pins */
    GPIO_ClockEnable(SAO_UART_TX_GPIO_PORT);
    GPIO_ClockEnable(SAO_UART_RX_GPIO_PORT);

    /* Enable USART2 clock (APB peripheral enable) */
    RCC->APBENR1 |= RCC_APBENR1_USART2EN;

    /* Configure GPIO pins for UART alternate function */
    GPIO_SetMode(SAO_UART_TX_GPIO_PORT, SAO_UART_TX_GPIO_PIN, GPIO_MODE_AF);
    GPIO_SetOutputType(SAO_UART_TX_GPIO_PORT, SAO_UART_TX_GPIO_PIN, GPIO_OTYPE_PP);
    GPIO_SetSpeed(SAO_UART_TX_GPIO_PORT, SAO_UART_TX_GPIO_PIN, GPIO_SPEED_HIGH);
    GPIO_SetPullUpDown(SAO_UART_TX_GPIO_PORT, SAO_UART_TX_GPIO_PIN, GPIO_PUPD_PU);
    GPIO_SetAlternateFunction(SAO_UART_TX_GPIO_PORT, SAO_UART_TX_GPIO_PIN, SAO_UART_TX_AF);

    GPIO_SetMode(SAO_UART_RX_GPIO_PORT, SAO_UART_RX_GPIO_PIN, GPIO_MODE_AF);
    GPIO_SetSpeed(SAO_UART_RX_GPIO_PORT, SAO_UART_RX_GPIO_PIN, GPIO_SPEED_HIGH);
    GPIO_SetPullUpDown(SAO_UART_RX_GPIO_PORT, SAO_UART_RX_GPIO_PIN, GPIO_PUPD_PU);
    GPIO_SetAlternateFunction(SAO_UART_RX_GPIO_PORT, SAO_UART_RX_GPIO_PIN, SAO_UART_RX_AF);

    /* Basic USART configuration: baudrate, 8N1 */
    USART2->CR1 = 0;
    USART2->CR2 = 0;
    USART2->CR3 = 0;

    uint32_t pclk = System_GetClock();
    USART2->BRR = pclk / UART_BAUDRATE;

    /* Enable transmitter and receiver */
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    /* Enable RX interrupt (RXNE/RXFIFO not empty) */
    USART2->CR1 |= USART_CR1_RXNEIE_RXFNEIE;
    NVIC_EnableIRQ(USART2_IRQn);

    uart2_enabled = true;
}

/* USART2 IRQ Handler - receives from SAO connector UART */
void USART2_IRQHandler(void) {
    if (USART2->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t d = USART2->RDR;
        uint32_t next = (uart_rx_head + 1) % UART_RX_BUFFER_SIZE;
        if (next != uart_rx_tail) {
            uart_rx_buffer[uart_rx_head] = d;
            uart_rx_head = next;
        }
    }
}
