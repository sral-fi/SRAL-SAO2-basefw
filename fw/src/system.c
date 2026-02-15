/* System initialization and clock configuration */

#include "system.h"
#include "config.h"
#include "stm32c011xx.h"
#include <stdint.h>

void System_ClockConfig(void) {
    /* Enable HSI (12 MHz internal RC oscillator) */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    /* HSI is already selected as system clock by default (RCC_CFGR_SW = 0) */
    /* No need to switch clock source since HSI is the default after reset */
}

uint32_t System_GetClock(void) {
    return SYSTEM_CLOCK_HZ;
}

void System_Init(void) {
    /* CMSIS SystemInit is called by startup code before main() */
    /* Configure system clock if needed beyond what SystemInit does */
    System_ClockConfig();
}

