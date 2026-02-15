/* Timer driver for microsecond/millisecond delays */

#include "timer.h"
#include "config.h"
#include "system.h"

#if 0
#include "stm32l475xx.h"

/* Software counter for TIM6 overflows (TIM6 is 16-bit) */
static uint32_t timer_overflow_count = 0;
static uint16_t last_timer_value = 0;

void Timer_Init(void) {
    /* Enable TIM6 clock for delay timing */
    /* TIM6 is on APB1 and won't conflict with PWM timers (TIM2, TIM3) */
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;

    /* Configure TIM6 for 1 MHz (1 μs tick) */
    /* TIM6 is on APB1 which runs at system clock (48 MHz) */
    /* TIM6 is a 16-bit timer, so we need to account for overflow */
    uint32_t timer_clock = System_GetClock();
    TIMER_PERIPHERAL->PSC = (timer_clock / DELAY_TIMER_FREQ_HZ) - 1;  /* Prescaler to get 1 MHz */
    TIMER_PERIPHERAL->ARR = 0xFFFF;  /* Maximum value for 16-bit counter */
    TIMER_PERIPHERAL->EGR = TIM_EGR_UG;  /* Generate update event to load prescaler */
    TIMER_PERIPHERAL->CR1 = TIM_CR1_CEN; /* Enable counter */
    
    timer_overflow_count = 0;
    last_timer_value = 0;
}

void delay_us(uint32_t us) {
    uint32_t start = TIMER_PERIPHERAL->CNT;
    while ((TIMER_PERIPHERAL->CNT - start) < us);
}

void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        delay_us(1000);
    }
}

uint32_t micros(void) {
    uint16_t current = TIMER_PERIPHERAL->CNT;
    
    /* Detect overflow by checking if current < last (wraparound occurred) */
    if (current < last_timer_value) {
        timer_overflow_count++;
    }
    last_timer_value = current;
    
    /* Return combined count: overflow_count in upper 16 bits, current timer in lower 16 bits */
    return (timer_overflow_count << 16) | current;
}

void PWM_Init(void) {
    /* Enable TIM2 and TIM3 clocks */
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN | RCC_APB1ENR1_TIM3EN;
    
    /* Enable GPIO clocks */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN;
    
    /* Configure TIM3 channels for PWM */
    /* TIM3_CH1: PA6 (LED2) */
    GPIOA->MODER &= ~(3U << (6 * 2));    /* Clear mode bits */
    GPIOA->MODER |= (2U << (6 * 2));     /* Set alternate function mode */
    GPIOA->AFR[0] &= ~(15U << (6 * 4));  /* Clear AF bits */
    GPIOA->AFR[0] |= (2U << (6 * 4));    /* Set AF2 for TIM3_CH1 */
    
    /* TIM3_CH2: PA7 (LED3) */
    GPIOA->MODER &= ~(3U << (7 * 2));    /* Clear mode bits */
    GPIOA->MODER |= (2U << (7 * 2));     /* Set alternate function mode */
    GPIOA->AFR[0] &= ~(15U << (7 * 4));  /* Clear AF bits */
    GPIOA->AFR[0] |= (2U << (7 * 4));    /* Set AF2 for TIM3_CH2 */
    
    /* TIM3_CH4: PC3 (LED1) */
    GPIOC->MODER &= ~(3U << (3 * 2));    /* Clear mode bits */
    GPIOC->MODER |= (2U << (3 * 2));     /* Set alternate function mode */
    GPIOC->AFR[0] &= ~(15U << (3 * 4));  /* Clear AF bits */
    GPIOC->AFR[0] |= (2U << (3 * 4));    /* Set AF2 for TIM3_CH4 */
    
    /* Configure TIM2 channels for PWM */
    /* TIM2_CH3: PB10 (LED4) */
    GPIOB->MODER &= ~(3U << (10 * 2));   /* Clear mode bits */
    GPIOB->MODER |= (2U << (10 * 2));    /* Set alternate function mode */
    GPIOB->AFR[1] &= ~(15U << ((10-8) * 4));  /* Clear AF bits (AFR[1] for pins 8-15) */
    GPIOB->AFR[1] |= (1U << ((10-8) * 4));    /* Set AF1 for TIM2_CH3 */
    
    /* Configure LED5 (PB11) as GPIO output since no more timers available */
    /* PB11 will use GPIO on/off toggling for blinking */
    GPIOB->MODER &= ~(3U << (11 * 2));   /* Clear mode bits */
    GPIOB->MODER |= (1U << (11 * 2));    /* Set output mode */
    GPIOB->OTYPER &= ~(1U << 11);        /* Push-pull */
    GPIOB->OSPEEDR |= (3U << (11 * 2));  /* High speed */
    GPIOB->PUPDR &= ~(3U << (11 * 2));   /* No pull-up/down */
    GPIOB->ODR &= ~(1U << 11);            /* Start with LED off */
    
    /* Set high speed and push-pull for all PWM pins */
    GPIOA->OSPEEDR |= (3U << (6 * 2)) | (3U << (7 * 2));  /* PA6, PA7 high speed */
    GPIOC->OSPEEDR |= (3U << (3 * 2));  /* PC3 high speed */
    GPIOB->OSPEEDR |= (3U << (10 * 2));  /* PB10 high speed */
    
    /* Configure TIM3 for PWM (100 kHz, 8-bit resolution) */
    TIM3->PSC = (System_GetClock() / 100000) - 1;  /* 100 kHz timer clock */
    TIM3->ARR = 255;  /* 8-bit PWM resolution (0-255) */
    TIM3->CCR1 = 0;   /* Start with 0% duty cycle */
    TIM3->CCR2 = 0;
    TIM3->CCR4 = 0;
    
    /* Configure TIM3 channels for PWM mode 1 */
    TIM3->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC2M);  /* Clear output compare modes */
    TIM3->CCMR1 |= (6U << TIM_CCMR1_OC1M_Pos) | (6U << TIM_CCMR1_OC2M_Pos);  /* PWM mode 1 */
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;  /* Enable preload */
    
    TIM3->CCMR2 &= ~TIM_CCMR2_OC4M;  /* Clear output compare mode */
    TIM3->CCMR2 |= (6U << TIM_CCMR2_OC4M_Pos);  /* PWM mode 1 */
    TIM3->CCMR2 |= TIM_CCMR2_OC4PE;  /* Enable preload */
    
    /* Enable TIM3 outputs */
    TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC4E;
    
    /* Enable TIM3 */
    TIM3->CR1 |= TIM_CR1_CEN;
    
    /* Configure TIM2 for PWM (100 kHz, 8-bit resolution) */
    TIM2->PSC = (System_GetClock() / 100000) - 1;  /* 100 kHz timer clock */
    TIM2->ARR = 255;  /* 8-bit PWM resolution (0-255) */
    TIM2->CCR3 = 0;   /* Start with 0% duty cycle */
    
    /* Configure TIM2 channels for PWM mode 1 */
    TIM2->CCMR2 &= ~TIM_CCMR2_OC3M;  /* Clear output compare mode */
    TIM2->CCMR2 |= (6U << TIM_CCMR2_OC3M_Pos);  /* PWM mode 1 */
    TIM2->CCMR2 |= TIM_CCMR2_OC3PE;  /* Enable preload */
    
    /* Enable TIM2 outputs */
    TIM2->CCER |= TIM_CCER_CC3E;
    
    /* Enable TIM2 */
    TIM2->CR1 |= TIM_CR1_CEN;
}

void PWM_SetDutyCycle(uint8_t channel, uint8_t duty_cycle) {
    switch (channel) {
        case 1:  /* LED1: TIM3_CH4 */
            TIM3->CCR4 = duty_cycle;
            break;
        case 2:  /* LED2: TIM3_CH1 */
            TIM3->CCR1 = duty_cycle;
            break;
        case 3:  /* LED3: TIM3_CH2 */
            TIM3->CCR2 = duty_cycle;
            break;
        case 4:  /* LED4: TIM2_CH3 */
            TIM2->CCR3 = duty_cycle;
            break;
        case 5:  /* LED5: GPIO on/off (PB11) - no timer available */
            if (duty_cycle > 128) {
                GPIOB->BSRR = (1U << 11);  /* Set pin high */
            } else {
                GPIOB->BSRR = (1U << (11 + 16));  /* Clear pin (using reset bits) */
            }
            break;
    }
}

#elif defined(BOARD_SRAL_SAO2)
#include "stm32c011xx.h"
#include "gpio.h"
#include "pins.h"
/* TODO: Implement for STM32C011 */

static volatile uint32_t systick_ms = 0;

/* SysTick-based millisecond tick for SRAL-SAO2 (STM32C0) */
void Timer_Init(void) {
    /* Configure SysTick to generate 1 ms interrupts using the core clock */
    uint32_t ticks = System_GetClock() / 1000U;
    if (ticks == 0) ticks = 1;

    /* LOAD is 24-bit on Cortex-M0+, ensure ticks fits */
    if (ticks > 0x00FFFFFFU) ticks = 0x00FFFFFFU;

    SysTick->LOAD = ticks - 1U;
    SysTick->VAL = 0U;
    /* CLKSOURCE = processor clock, TICKINT = enable, ENABLE = enable counter */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

void SysTick_Handler(void) {
    systick_ms++;
}

void delay_us(uint32_t us) {
    /* Approximate microsecond delay using SysTick counter resolution (1 ms).
       For sub-ms accuracy this is a simple busy-wait calibrated loop fallback. */
    if (us >= 1000U) {
        uint32_t ms = us / 1000U;
        delay_ms(ms);
        us = us % 1000U;
    }

    /* Short busy-wait calibrated loop for remaining microseconds.
       Calibration: assume core ~ SYSTEM_CLOCK_HZ; simple tight loop */
    volatile uint32_t count = (System_GetClock() / 1000000U) * us / 6U; /* rough cycles/loop */
    while (count--) {
        __asm volatile ("nop");
    }
}

void delay_ms(uint32_t ms) {
    uint32_t start = systick_ms;
    while ((systick_ms - start) < ms) {
        /* busy-wait until required ms have elapsed */
    }
}

uint32_t micros(void) {
    /* Return milliseconds * 1000 (approx). Not exact sub-ms value. */
    return systick_ms * 1000U;
}

/* Minimal PWM stubs for SRAL_SAO2 to satisfy references from main/CLI.
   These provide simple on/off control using GPIO pins since the
   small STM32C0 device may not have the same timers configured yet. */
void PWM_Init(void) {
    /* Enable GPIO clocks for LED ports and configure pins as outputs */
    GPIO_ClockEnable(LED1_GPIO_PORT);
    GPIO_ClockEnable(LED2_GPIO_PORT);
    GPIO_ClockEnable(LED3_GPIO_PORT);
    GPIO_ClockEnable(LED4_GPIO_PORT);
    GPIO_ClockEnable(LED5_GPIO_PORT);

    GPIO_SetMode(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED3_GPIO_PORT, LED3_GPIO_PIN, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED4_GPIO_PORT, LED4_GPIO_PIN, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED5_GPIO_PORT, LED5_GPIO_PIN, GPIO_MODE_OUTPUT);
}

void PWM_SetDutyCycle(uint8_t channel, uint8_t duty_cycle) {
    /* Simple threshold-based on/off PWM emulation */
    (void)duty_cycle;
    switch (channel) {
        case 1:
            if (duty_cycle > 128) GPIO_SetPin(LED1_GPIO_PORT, LED1_GPIO_PIN);
            else GPIO_ClearPin(LED1_GPIO_PORT, LED1_GPIO_PIN);
            break;
        case 2:
            if (duty_cycle > 128) GPIO_SetPin(LED2_GPIO_PORT, LED2_GPIO_PIN);
            else GPIO_ClearPin(LED2_GPIO_PORT, LED2_GPIO_PIN);
            break;
        case 3:
            if (duty_cycle > 128) GPIO_SetPin(LED3_GPIO_PORT, LED3_GPIO_PIN);
            else GPIO_ClearPin(LED3_GPIO_PORT, LED3_GPIO_PIN);
            break;
        case 4:
            if (duty_cycle > 128) GPIO_SetPin(LED4_GPIO_PORT, LED4_GPIO_PIN);
            else GPIO_ClearPin(LED4_GPIO_PORT, LED4_GPIO_PIN);
            break;
        case 5:
            if (duty_cycle > 128) GPIO_SetPin(LED5_GPIO_PORT, LED5_GPIO_PIN);
            else GPIO_ClearPin(LED5_GPIO_PORT, LED5_GPIO_PIN);
            break;
        default:
            break;
    }
}

#endif
