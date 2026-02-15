#ifndef CONFIG_H
#define CONFIG_H

/* STM32C011F6P6 Board Configuration */

/* Feature flags */
#define ENABLE_HAMQUEST 1  /* Disable hamquest to save memory */

/* Clock Configuration */
#define HSI_VALUE           12000000U   /* HSI oscillator frequency */
#define SYSTEM_CLOCK_HZ     12000000U   /* Target system clock: 12 MHz */

/* UART Configuration */
#define UART_BAUDRATE       115200U
/* Define UART peripheral and IRQ for this board */
#define UART_PERIPHERAL     USART1
#define UART_IRQn           USART1_IRQn

/* Timer Configuration */
/* TODO: Define TIMER_PERIPHERAL after CMSIS is available */
#define DELAY_TIMER_FREQ_HZ 1000000U    /* 1 MHz for microsecond delays */

/* Buffer Sizes */
#define UART_TX_BUFFER_SIZE 128         /* Smaller buffers for limited RAM */
#define UART_RX_BUFFER_SIZE 128
#define CLI_BUFFER_SIZE     80

#endif /* CONFIG_H */
