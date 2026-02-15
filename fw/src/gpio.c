/* GPIO driver - bare metal implementation */

#include "gpio.h"
#include "stm32c011xx.h"

/* Enable GPIO clocks for STM32C0 series (IOPENR register) */
void GPIO_ClockEnable(void *port) {
    if (port == GPIOA) RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
    else if (port == GPIOB) RCC->IOPENR |= RCC_IOPENR_GPIOBEN;
    else if (port == GPIOC) RCC->IOPENR |= RCC_IOPENR_GPIOCEN;
    else {
        /* Some small devices may not have all ports; ignore unknown ports */
        (void)port;
    }
}

void GPIO_SetMode(void *port, uint8_t pin, GPIO_Mode_t mode) {
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    gpio->MODER &= ~(3U << (pin * 2));
    gpio->MODER |= (mode << (pin * 2));
}

void GPIO_SetOutputType(void *port, uint8_t pin, GPIO_OType_t otype) {
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    if (otype == GPIO_OTYPE_OD) {
        gpio->OTYPER |= (1U << pin);
    } else {
        gpio->OTYPER &= ~(1U << pin);
    }
}

void GPIO_SetSpeed(void *port, uint8_t pin, GPIO_Speed_t speed) {
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    gpio->OSPEEDR &= ~(3U << (pin * 2));
    gpio->OSPEEDR |= (speed << (pin * 2));
}

void GPIO_SetPullUpDown(void *port, uint8_t pin, GPIO_PuPd_t pupd) {
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    gpio->PUPDR &= ~(3U << (pin * 2));
    gpio->PUPDR |= (pupd << (pin * 2));
}

void GPIO_SetAlternateFunction(void *port, uint8_t pin, uint8_t af) {
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    uint8_t reg_idx = pin / 8;
    uint8_t bit_pos = (pin % 8) * 4;
    gpio->AFR[reg_idx] &= ~(0xFU << bit_pos);
    gpio->AFR[reg_idx] |= (af << bit_pos);
}

void GPIO_SetPin(void *port, uint8_t pin) {
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    gpio->BSRR = (1U << pin);
}

void GPIO_ClearPin(void *port, uint8_t pin) {
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    gpio->BSRR = (1U << (pin + 16));
}

void GPIO_TogglePin(void *port, uint8_t pin) {
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    gpio->ODR ^= (1U << pin);
}

uint8_t GPIO_ReadPin(void *port, uint8_t pin) {
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    return (gpio->IDR & (1U << pin)) ? 1 : 0;
}
