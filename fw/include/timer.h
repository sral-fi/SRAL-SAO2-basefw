#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Timer initialization */
void Timer_Init(void);

/* Delay functions */
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

/* Get microsecond timestamp */
uint32_t micros(void);

/* PWM functions */
void PWM_Init(void);
void PWM_SetDutyCycle(uint8_t channel, uint8_t duty_cycle);

#endif /* TIMER_H */
