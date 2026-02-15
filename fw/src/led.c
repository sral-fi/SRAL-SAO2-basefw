/* LED control functions */

#include "led.h"
#include "gpio.h"
#include "pins.h"
#include "timer.h"

#include "stm32c011xx.h"

static LED_Mode_t led_mode = LED_MODE_OFF;
static uint32_t last_blink_time = 0;

/* LED auto-blink mode names: 0=OFF, 1=BLINK, 2=FADE, 3=CW, 4=STROBO, 5=ICIRCLE, 6=DISCO */
const char *led_blink_mode_names[7] = {"OFF", "BLINK", "FADE", "CW", "STROBO", "ICIRCLE", "DISCO"};

void LED_Init(void) {
    /* Enable GPIO clock */
    GPIO_ClockEnable(LED_GPIO_PORT);
    
    /* Configure LED pin as output */
    GPIO_SetMode(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    GPIO_SetOutputType(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_OTYPE_PP);
    GPIO_SetSpeed(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_SPEED_LOW);
    GPIO_SetPullUpDown(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PUPD_NONE);
    
    /* Start with LED off */
    LED_Off();
}

void LED_On(void) {
    GPIO_SetPin(LED_GPIO_PORT, LED_GPIO_PIN);
    led_mode = LED_MODE_ON;
}

void LED_Off(void) {
    GPIO_ClearPin(LED_GPIO_PORT, LED_GPIO_PIN);
    led_mode = LED_MODE_OFF;
}

void LED_Toggle(void) {
    GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
}

void LED_SetMode(LED_Mode_t mode) {
    led_mode = mode;
    last_blink_time = micros();
    if (mode == LED_MODE_OFF) {
        LED_Off();
    } else if (mode == LED_MODE_ON) {
        LED_On();
    } else if (mode == LED_MODE_BLINK) {
        LED_On(); // Start with LED on
    }
}

LED_Mode_t LED_GetMode(void) {
    return led_mode;
}

void LED_Update(void) {
    if (led_mode == LED_MODE_BLINK) {
        uint32_t now = micros();
        // Toggle every 500ms (500000 microseconds)
        if (now - last_blink_time >= 500000) {
            last_blink_time = now;
            LED_Toggle();
        }
    }
}
