#ifndef LED_H
#define LED_H

#include <stdint.h>

/* LED modes */
typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_ON,
    LED_MODE_BLINK
} LED_Mode_t;

/* LED auto-blink mode names: 0=OFF, 1=BLINK, 2=FADE, 3=CW, 4=STROBO, 5=ICIRCLE, 6=DISCO */
extern const char *led_blink_mode_names[7];

/* LED functions */
void LED_Init(void);
void LED_On(void);
void LED_Off(void);
void LED_Toggle(void);
void LED_SetMode(LED_Mode_t mode);
LED_Mode_t LED_GetMode(void);
void LED_Update(void);  /* Call periodically (e.g., every 1ms) for blink mode */

#endif /* LED_H */
