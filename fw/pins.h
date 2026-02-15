/* SRAL-SAO2 board (STM32C011F6P6) pin definitions */

#ifndef PINS_H
#define PINS_H

/* BUTTON PIN PA2 */
#define BTN_GPIO_PORT      GPIOA
#define BTN_GPIO_PIN       2           /* PA2 - BUTTON */

/* BADGE_PWR_SENSE PB6 */
#define BADGE_PWR_SENSE_GPIO_PORT GPIOB
#define BADGE_PWR_SENSE_GPIO_PIN  6           /* PB6 - BADGE_PWR_SENSE */

/* LED Pin */
#define LED_GPIO_PORT       GPIOA
#define LED_GPIO_PIN        6           /* PA6 */

// LED1...LED5 pins:
// LED1: PCPA8
#define LED1_GPIO_PORT     GPIOA
#define LED1_GPIO_PIN      8           /* PA8 - LED1 */
// LED2: PA5
#define LED2_GPIO_PORT     GPIOA
#define LED2_GPIO_PIN      5           /* PA5 - LED2 */
// LED3: PB7
#define LED3_GPIO_PORT     GPIOB
#define LED3_GPIO_PIN      7           /* PB7 - LED3 */
// LED4: PC15
#define LED4_GPIO_PORT     GPIOC
#define LED4_GPIO_PIN      14          /* PC14 - LED4 */
// LED5: PC14
#define LED5_GPIO_PORT     GPIOC
#define LED5_GPIO_PIN      15          /* PC15 - LED5 */

/* UART Pins (USART1 -- HEADER) */
#define UART_TX_GPIO_PORT   GPIOA
#define UART_TX_GPIO_PIN    0           /* PA0 - USART2_TX */
#define UART_TX_AF          4           /* AF4 for USART1 */

#define UART_RX_GPIO_PORT   GPIOA
#define UART_RX_GPIO_PIN    1           /* PA1 - USART2_RX */
#define UART_RX_AF          4           /* AF4 for USART1 */

/* UART Pins (USART2 -- SAO_CONN) */
#define SAO_UART_TX_GPIO_PORT   GPIOA
#define SAO_UART_TX_GPIO_PIN    4           /* PA4 - USART2_TX */
#define SAO_UART_TX_AF          1           /* AF1 for USART2 */

#define SAO_UART_RX_GPIO_PORT   GPIOA
#define SAO_UART_RX_GPIO_PIN    3           /* PA1 - USART2_RX */
#define SAO_UART_RX_AF          1           /* AF1 for USART2 */

/* I2C Pins (I2C1 - 24C02 EEPROM) */
/* Note: PA9/PA10 are remapped to PA11/PA12 in TSSOP20 package */
#define I2C_SCL_GPIO_PORT       GPIOA
#define I2C_SCL_GPIO_PIN        9           /* PA9 (remapped to PA11 physical) - I2C1_SCL */
#define I2C_SCL_AF              6           /* AF6 for I2C1 */

#define I2C_SDA_GPIO_PORT       GPIOA
#define I2C_SDA_GPIO_PIN        10          /* PA10 (remapped to PA12 physical) - I2C1_SDA */
#define I2C_SDA_AF              6           /* AF6 for I2C1 */

/* I2C Internal Pull-up Configuration */
#define I2C_USE_INTERNAL_PULLUPS 1          /* 0 = disable (use external pull-ups), 1 = enable internal pull-ups */

/* 24C02 EEPROM I2C Address */
#define EEPROM_I2C_ADDR         0xA0        /* 24C02 default address (7-bit: 0x50) */

#endif /* PINS_H */
