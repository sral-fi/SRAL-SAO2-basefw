#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

/* GPIO Mode */
typedef enum {
    GPIO_MODE_INPUT  = 0,
    GPIO_MODE_OUTPUT = 1,
    GPIO_MODE_AF     = 2,
    GPIO_MODE_ANALOG = 3
} GPIO_Mode_t;

/* GPIO Output Type */
typedef enum {
    GPIO_OTYPE_PP = 0,  /* Push-pull */
    GPIO_OTYPE_OD = 1   /* Open-drain */
} GPIO_OType_t;

/* GPIO Speed */
typedef enum {
    GPIO_SPEED_LOW    = 0,
    GPIO_SPEED_MEDIUM = 1,
    GPIO_SPEED_HIGH   = 2,
    GPIO_SPEED_VERY_HIGH = 3
} GPIO_Speed_t;

/* GPIO Pull-up/Pull-down */
typedef enum {
    GPIO_PUPD_NONE = 0,
    GPIO_PUPD_PU   = 1,  /* Pull-up */
    GPIO_PUPD_PD   = 2   /* Pull-down */
} GPIO_PuPd_t;

/* GPIO functions */
void GPIO_ClockEnable(void *port);
void GPIO_SetMode(void *port, uint8_t pin, GPIO_Mode_t mode);
void GPIO_SetOutputType(void *port, uint8_t pin, GPIO_OType_t otype);
void GPIO_SetSpeed(void *port, uint8_t pin, GPIO_Speed_t speed);
void GPIO_SetPullUpDown(void *port, uint8_t pin, GPIO_PuPd_t pupd);
void GPIO_SetAlternateFunction(void *port, uint8_t pin, uint8_t af);
void GPIO_SetPin(void *port, uint8_t pin);
void GPIO_ClearPin(void *port, uint8_t pin);
void GPIO_TogglePin(void *port, uint8_t pin);
uint8_t GPIO_ReadPin(void *port, uint8_t pin);

#endif /* GPIO_H */
