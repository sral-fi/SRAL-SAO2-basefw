/* Main application with CLI integration */

#include "system.h"
#include "gpio.h"
#include "uart.h"
#include "timer.h"
#include "led.h"
#include "cli.h"
#include "pins.h"
#include "config.h"
#include "stm32c0xx.h"
#include "i2c_eeprom.h"
#include <stddef.h>
#include <stdbool.h>

/* Individual LED blinking system */
bool led_blinking[5] = {false, false, false, false, false};  /* LED1-LED5 */
uint32_t led_blink_times[5] = {0, 0, 0, 0, 0};

/* Debug LED blink control */
bool debug_led_blinking = false;
uint32_t debug_led_blink_time = 0;
uint8_t led_brightness[5] = {0, 0, 0, 0, 0};  /* Current PWM brightness 0-255 */
bool led_fade_direction[5] = {true, true, true, true, true};  /* true = fading up, false = fading down */

/* LED auto-blink mode: 0=OFF, 1=BLINK, 2=FADE, 3=CW, 4=STROBO, 5=ICIRCLE, 6=DISCO */
volatile uint8_t led_auto_mode = 1; /* default to BLINK */
static uint32_t lcg_state = 0xA5A5A5A5UL; /* Simple LCG PRNG state */
static int last_blink_idx = -1; /* Remember last blinked LED to avoid repeating */
static volatile uint8_t button_interrupt_flag = 0; /* Button press flag */

/* Simple LCG pseudo-random number generator */
static uint32_t lcg_rand(void) {
    lcg_state = lcg_state * 1664525UL + 1013904223UL;
    return lcg_state;
}

int main(void) {
    /* Initialize system first */
    System_Init();
    Timer_Init();
    
    /* Configure button pin early to check if it's held during boot */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN; /* Ensure GPIOA clock enabled */
    GPIOA->MODER &= ~(3UL << (BTN_GPIO_PIN * 2));  /* Input mode (00) */
    GPIOA->PUPDR &= ~(3UL << (BTN_GPIO_PIN * 2));
    GPIOA->PUPDR |= (1UL << (BTN_GPIO_PIN * 2));   /* Pull-up */
    
    /* Small delay to let the pull-up stabilize */
    for (volatile int i = 0; i < 1000; i++);
    
    /* Check if button is held (active low) */
    bool button_held = ((GPIOA->IDR & (1UL << BTN_GPIO_PIN)) == 0);
    
    /* Initialize UARTs */
    UART_Init();
    if (button_held) {
        UART2_Init();  /* Enable SAO connector UART if button held */
    }
    
    LED_Init();
    
    /* Turn on debug LED to indicate boot in progress */
    LED_SetMode(LED_MODE_ON);
    
     /* Ensure SYSCFG remap for PA11/PA12 so I2C1 uses the physical pins
         that have the external 4.7k pull-ups on this board. This must be
         done before `eeprom_init()` configures GPIO/I2C. */
     RCC->APBENR2 |= RCC_APBENR2_SYSCFGEN;
     SYSCFG->CFGR1 |= (SYSCFG_CFGR1_PA11_RMP | SYSCFG_CFGR1_PA12_RMP);

     /* Initialize EEPROM for non-volatile storage */
     eeprom_init();
    
    /* Initialize additional LEDs (LED1-LED5) */
    void *led_ports[5] = { 
        (void *)LED1_GPIO_PORT, 
        (void *)LED2_GPIO_PORT, 
        (void *)LED3_GPIO_PORT, 
        (void *)LED4_GPIO_PORT, 
        (void *)LED5_GPIO_PORT 
    };
    uint8_t led_pins[5] = { 
        LED1_GPIO_PIN, 
        LED2_GPIO_PIN, 
        LED3_GPIO_PIN, 
        LED4_GPIO_PIN, 
        LED5_GPIO_PIN 
    };
    
    for (int i = 0; i < 5; i++) {
        GPIO_ClockEnable(led_ports[i]);
        GPIO_SetMode(led_ports[i], led_pins[i], GPIO_MODE_OUTPUT);
        GPIO_SetOutputType(led_ports[i], led_pins[i], GPIO_OTYPE_PP);
        GPIO_SetSpeed(led_ports[i], led_pins[i], GPIO_SPEED_HIGH);
        GPIO_SetPullUpDown(led_ports[i], led_pins[i], GPIO_PUPD_NONE);
        GPIO_ClearPin(led_ports[i], led_pins[i]);
    }
    
    /* Configure BADGE_PWR_SENSE pin (PB6) as input with pull-down */
    RCC->IOPENR |= RCC_IOPENR_GPIOBEN; /* Enable GPIOB clock */
    GPIOB->MODER &= ~(3UL << (BADGE_PWR_SENSE_GPIO_PIN * 2));  /* Input mode (00) */
    GPIOB->PUPDR &= ~(3UL << (BADGE_PWR_SENSE_GPIO_PIN * 2));  /* Clear PUPDR bits */
    GPIOB->PUPDR |= (2UL << (BADGE_PWR_SENSE_GPIO_PIN * 2));   /* Pull-down (10) */
    
    /* Configure EXTI interrupt for button (PA2 already configured earlier) */
    RCC->APBENR2 |= RCC_APBENR2_SYSCFGEN; /* Enable SYSCFG clock */
    /* Configure EXTI2 for button on PA2 */
    EXTI->EXTICR[0] &= ~(0xFFUL << 16);  /* EXTI2 is in EXTICR[0], bits 16-23. Value 0 is GPIOA */
    EXTI->IMR1 |= (1UL << 2);   /* Unmask EXTI2 */
    EXTI->FTSR1 |= (1UL << 2);  /* Falling edge (button press) */
    EXTI->RTSR1 &= ~(1UL << 2); /* Disable rising edge */
    EXTI->RPR1 = (1UL << 2);    /* Clear any existing pending bits */
    EXTI->FPR1 = (1UL << 2);
    NVIC_ClearPendingIRQ(EXTI2_3_IRQn);
    NVIC_EnableIRQ(EXTI2_3_IRQn);
    
    /* Initialize CLI */
    CLI_Init();
    CLI_SetBootTime();
    CLI_ShowBootMessages(true);
    CLI_PrintPrompt();
    
    /* Turn off debug LED now that boot is complete */
    LED_SetMode(LED_MODE_OFF);
    
    /* Main loop */
    while (1) {
        /* Check button interrupt flag for mode cycling */
        if (button_interrupt_flag) {
            button_interrupt_flag = 0;
            delay_us(50000); /* Simple debounce: wait 50ms */
            button_interrupt_flag = 0;
            
            /* Cycle through modes: 0->1->2->3->4->5->6->0 */
            led_auto_mode = (led_auto_mode + 1) % 7;
            
            /* Ensure mode stays in valid range */
            if (led_auto_mode >= 7) led_auto_mode = 0;
            
            /* Report mode change to console */
            UART_SendString("\r\nAuto-blink mode changed to: ");
            UART_SendString(led_blink_mode_names[led_auto_mode]);
            UART_SendString("\r\n");
            // print current prompt again:
            CLI_PrintPrompt();
            
            /* Turn off all LEDs when entering OFF mode */
            if (led_auto_mode == 0) {
                for (int i = 0; i < 5; i++) {
                    GPIO_ClearPin(led_ports[i], led_pins[i]);
                }
            }
        }
        
        /* Handle auto LED modes */
        if (led_auto_mode == 1) { /* BLINK mode */
            uint32_t r = lcg_rand();
            uint32_t idx = r % 5; /* 0..4 -> LED1..LED5 */
            /* Ensure we don't pick the same LED twice in a row */
            if ((int)idx == last_blink_idx) {
                idx = (idx + 1) % 5;
            }
            last_blink_idx = (int)idx;
            
            /* Turn selected LED ON */
            GPIO_SetPin(led_ports[idx], led_pins[idx]);
            
            /* Randomized on-time: 10..80 ms, split into chunks to remain responsive */
            uint32_t on_ms = 10 + (lcg_rand() % 71);
            const uint32_t chunk_ms = 10;
            uint32_t loops = on_ms / chunk_ms;
            for (uint32_t t = 0; t < loops; ++t) {
                char c;
                if (UART_ReceiveChar(&c)) {
                    CLI_ProcessChar(c);
                }
                if (button_interrupt_flag) break;
                delay_us(chunk_ms * 1000);
            }
            if (button_interrupt_flag) continue;
            
            /* Turn selected LED OFF */
            GPIO_ClearPin(led_ports[idx], led_pins[idx]);
            
            /* Randomized gap: 50..1050 ms before next blink */
            uint32_t gap_ms = 50 + (lcg_rand() % 1001);
            loops = gap_ms / 25;
            for (uint32_t t = 0; t < loops; ++t) {
                char c;
                if (UART_ReceiveChar(&c)) {
                    CLI_ProcessChar(c);
                }
                if (button_interrupt_flag) break;
                delay_us(25000);
            }
            if (button_interrupt_flag) continue;
        } else if (led_auto_mode == 2) { /* FADE mode */
            uint32_t r = lcg_rand();
            uint32_t idx = r % 5;
            if ((int)idx == last_blink_idx) {
                idx = (idx + 1) % 5;
            }
            last_blink_idx = (int)idx;
            
            /* Turn LED fully on */
            GPIO_SetPin(led_ports[idx], led_pins[idx]);
            
            /* Hold at full brightness for 100ms */
            for (uint32_t t = 0; t < 10; ++t) {
                char c;
                if (UART_ReceiveChar(&c)) {
                    CLI_ProcessChar(c);
                }
                if (button_interrupt_flag) break;
                delay_us(10000);
            }
            if (button_interrupt_flag) {
                GPIO_ClearPin(led_ports[idx], led_pins[idx]);
                continue;
            }
            
            /* Fade out using PWM-like effect: progressively reduce on-time */
            /* 20 fade steps, each step gets dimmer */
            for (uint32_t fade_step = 20; fade_step > 0; --fade_step) {
                /* duty_cycle: fade_step/20 = brightness percentage */
                uint32_t on_time_us = (fade_step * 50);  /* 50us per step = 1000us max */
                uint32_t off_time_us = 1000 - on_time_us;
                
                /* Repeat this duty cycle for ~15ms total per fade step */
                for (uint32_t cycle = 0; cycle < 15; ++cycle) {
                    GPIO_SetPin(led_ports[idx], led_pins[idx]);
                    delay_us(on_time_us);
                    GPIO_ClearPin(led_ports[idx], led_pins[idx]);
                    if (off_time_us > 0) {
                        delay_us(off_time_us);
                    }
                    
                    /* Check for interrupts every few cycles */
                    if (cycle % 3 == 0) {
                        char c;
                        if (UART_ReceiveChar(&c)) {
                            CLI_ProcessChar(c);
                        }
                        if (button_interrupt_flag) break;
                    }
                }
                if (button_interrupt_flag) break;
            }
            
            /* Ensure LED is fully off */
            GPIO_ClearPin(led_ports[idx], led_pins[idx]);
            if (button_interrupt_flag) continue;
            
            /* Random gap before next LED: 200..1200 ms */
            uint32_t gap_ms = 200 + (lcg_rand() % 1001);
            uint32_t loops = gap_ms / 25;
            for (uint32_t t = 0; t < loops; ++t) {
                char c;
                if (UART_ReceiveChar(&c)) {
                    CLI_ProcessChar(c);
                }
                if (button_interrupt_flag) break;
                delay_us(25000);
            }
            if (button_interrupt_flag) continue;
        } else if (led_auto_mode == 3) { /* CW mode - send Morse by blinking LEDs */
            /* CW timing unit in ms */
            const uint32_t UNIT_MS = 100;
            /* LEDs used: dit = LED1+LED2 (indices 0,1), dash = LED3+LED4 (indices 2,3) */
            /* Transmit current_cw repeatedly with clear pause between repeats */
            if (current_cw[0] == '\0') {
                /* Nothing to send: small sleep */
                for (uint32_t t = 0; t < 10; ++t) {
                    char c;
                    if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                    if (button_interrupt_flag) break;
                    delay_us(UNIT_MS * 1000 / 10);
                }
                continue;
            }

            /* Iterate characters in CW message */
            for (size_t ci = 0; current_cw[ci] != '\0'; ++ci) {
                char ch = current_cw[ci];
                if (ch == ' ') {
                    /* word gap: 9 units (increased) */
                    for (uint32_t g = 0; g < (9 * UNIT_MS) / 10; ++g) {
                        char c;
                        if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                        if (button_interrupt_flag || led_auto_mode != 3) break;
                        delay_us(10 * 1000);
                    }
                    if (button_interrupt_flag || led_auto_mode != 3) break;
                    continue;
                }

                /* Lookup Morse pattern for ch (A-Z,0-9). Use uppercase */
                char upper = (ch >= 'a' && ch <= 'z') ? (ch - 32) : ch;
                const char *morse = NULL;
                switch (upper) {
                    /* Letters */
                    case 'A': morse = ".-"; break;
                    case 'B': morse = "-..."; break;
                    case 'C': morse = "-.-."; break;
                    case 'D': morse = "-.."; break;
                    case 'E': morse = "."; break;
                    case 'F': morse = "..-."; break;
                    case 'G': morse = "--."; break;
                    case 'H': morse = "...."; break;
                    case 'I': morse = ".."; break;
                    case 'J': morse = ".---"; break;
                    case 'K': morse = "-.-"; break;
                    case 'L': morse = ".-.."; break;
                    case 'M': morse = "--"; break;
                    case 'N': morse = "-."; break;
                    case 'O': morse = "---"; break;
                    case 'P': morse = ".--."; break;
                    case 'Q': morse = "--.-"; break;
                    case 'R': morse = ".-."; break;
                    case 'S': morse = "..."; break;
                    case 'T': morse = "-"; break;
                    case 'U': morse = "..-"; break;
                    case 'V': morse = "...-"; break;
                    case 'W': morse = ".--"; break;
                    case 'X': morse = "-..-"; break;
                    case 'Y': morse = "-.--"; break;
                    case 'Z': morse = "--.."; break;
                    /* Numbers */
                    case '0': morse = "-----"; break;
                    case '1': morse = ".----"; break;
                    case '2': morse = "..---"; break;
                    case '3': morse = "...--"; break;
                    case '4': morse = "....-"; break;
                    case '5': morse = "....."; break;
                    case '6': morse = "-...."; break;
                    case '7': morse = "--..."; break;
                    case '8': morse = "---.."; break;
                    case '9': morse = "----."; break;
                    default: morse = NULL; break;
                }
                if (!morse) continue; /* skip unsupported char */

                /* For each symbol in morse string */
                for (const char *p = morse; *p != '\0'; ++p) {
                    if (button_interrupt_flag || led_auto_mode != 3) break;
                    if (*p == '.') {
                        /* dit: LEDs 5+1 on for 1 unit */
                        GPIO_SetPin(led_ports[4], led_pins[4]);
                        GPIO_SetPin(led_ports[0], led_pins[0]);
                        uint32_t units = UNIT_MS;
                        for (uint32_t t = 0; t < units / 25; ++t) {
                            char c;
                            if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                            if (button_interrupt_flag || led_auto_mode != 3) break;
                            delay_us(25000);
                        }
                        GPIO_ClearPin(led_ports[4], led_pins[4]);
                        GPIO_ClearPin(led_ports[0], led_pins[0]);
                    } else if (*p == '-') {
                        /* dash: LEDs 2+3+4 on for 3 units */
                        GPIO_SetPin(led_ports[3], led_pins[3]);
                        GPIO_SetPin(led_ports[2], led_pins[2]);
                        GPIO_SetPin(led_ports[1], led_pins[1]);
                        uint32_t units = 3 * UNIT_MS;
                        for (uint32_t t = 0; t < units / 25; ++t) {
                            char c;
                            if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                            if (button_interrupt_flag || led_auto_mode != 3) break;
                            delay_us(25000);
                        }
                        GPIO_ClearPin(led_ports[3], led_pins[3]);
                        GPIO_ClearPin(led_ports[2], led_pins[2]);
                        GPIO_ClearPin(led_ports[1], led_pins[1]);
                    }

                    /* inter-element gap: 1 unit (already had element off for small time) */
                    for (uint32_t g = 0; g < UNIT_MS / 25; ++g) {
                        char c;
                        if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                        if (button_interrupt_flag || led_auto_mode != 3) break;
                        delay_us(25000);
                    }
                    if (button_interrupt_flag || led_auto_mode != 3) break;
                }
                if (button_interrupt_flag || led_auto_mode != 3) break;

                /* inter-character gap: 5 units total, 1+4 */
                for (uint32_t g = 0; g < (4 * UNIT_MS) / 25; ++g) {
                    char c;
                    if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                    if (button_interrupt_flag || led_auto_mode != 3) break;
                    delay_us(25000);
                }
                if (button_interrupt_flag || led_auto_mode != 3) break;
            }

            /* After finishing message once, pause before repeating: 9 units (increased) */
            for (uint32_t g = 0; g < (9 * UNIT_MS) / 25; ++g) {
                char c;
                if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                if (button_interrupt_flag || led_auto_mode != 3) break;
                delay_us(25000);
            }
            if (button_interrupt_flag) continue;

    } else if (led_auto_mode == 4) { /* STROBO mode - disco mode with intensive rhythm */
            /* Pick 2-4 random LEDs */
            uint32_t num_leds = 2 + (lcg_rand() % 3); /* 2, 3, or 4 LEDs */
            bool led_active[5] = {false, false, false, false, false};
            
            for (uint32_t i = 0; i < num_leds; i++) {
                uint32_t idx = lcg_rand() % 5;
                led_active[idx] = true;
            }
            
            /* Fast strobo sequence: 3-6 rapid flashes */
            uint32_t flashes = 3 + (lcg_rand() % 4);
            for (uint32_t flash = 0; flash < flashes; flash++) {
                /* Turn selected LEDs ON */
                for (int i = 0; i < 5; i++) {
                    if (led_active[i]) {
                        GPIO_SetPin(led_ports[i], led_pins[i]);
                    }
                }
                
                /* Very short on-time: 30-70ms */
                uint32_t on_ms = 30 + (lcg_rand() % 41);
                for (uint32_t t = 0; t < on_ms / 10; ++t) {
                    char c;
                    if (UART_ReceiveChar(&c)) {
                        CLI_ProcessChar(c);
                    }
                    if (button_interrupt_flag) break;
                    delay_us(10000);
                }
                if (button_interrupt_flag) break;
                
                /* Turn selected LEDs OFF */
                for (int i = 0; i < 5; i++) {
                    if (led_active[i]) {
                        GPIO_ClearPin(led_ports[i], led_pins[i]);
                    }
                }
                
                /* Very short gap between flashes: 40-90ms */
                uint32_t gap_ms = 40 + (lcg_rand() % 51);
                for (uint32_t t = 0; t < gap_ms / 10; ++t) {
                    char c;
                    if (UART_ReceiveChar(&c)) {
                        CLI_ProcessChar(c);
                    }
                    if (button_interrupt_flag) break;
                    delay_us(10000);
                }
                if (button_interrupt_flag) break;
            }
            if (button_interrupt_flag) continue;
            
            /* Short pause before next strobo sequence: 100-300ms */
            uint32_t pause_ms = 100 + (lcg_rand() % 201);
            uint32_t loops = pause_ms / 25;
            for (uint32_t t = 0; t < loops; ++t) {
                char c;
                if (UART_ReceiveChar(&c)) {
                    CLI_ProcessChar(c);
                }
                if (button_interrupt_flag) break;
                delay_us(25000);
            }
            if (button_interrupt_flag) continue;
    } else if (led_auto_mode == 5) { /* ICIRCLE mode - circular LED pattern with direction changes, acceleration, and random blinks */
            static uint8_t circle_step = 0;
            static bool circle_forward = true;
            static uint8_t direction_counter = 0;
            static uint8_t speed_step = 0; /* Track steps for acceleration */
            const uint8_t STEPS_PER_DIRECTION = 10; /* Number of steps before reversing */
            
            /* Calculate speed - accelerates over time, max speed at 25 steps */
            uint32_t base_delay = 100; /* Start at 100ms */
            if (speed_step < 25) {
                speed_step++;
                base_delay = 100 - (speed_step * 3); /* Accelerate: 100ms -> 25ms */
            } else {
                base_delay = 25; /* Max speed */
            }
            
            /* Occasionally add a double-blink to break the pattern */
            bool add_blink = (lcg_rand() % 8 == 0); /* ~12% chance */
            
            if (add_blink) {
                /* Quick double-blink on a random LED */
                uint8_t blink_led = lcg_rand() % 5;
                for (uint8_t blinks = 0; blinks < 2; blinks++) {
                    GPIO_SetPin(led_ports[blink_led], led_pins[blink_led]);
                    for (uint32_t t = 0; t < 5; ++t) {
                        char c;
                        if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                        if (button_interrupt_flag) break;
                        delay_us(10000);
                    }
                    GPIO_ClearPin(led_ports[blink_led], led_pins[blink_led]);
                    if (button_interrupt_flag) continue;
                    for (uint32_t t = 0; t < 3; ++t) {
                        char c;
                        if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                        if (button_interrupt_flag) break;
                        delay_us(10000);
                    }
                    if (button_interrupt_flag) break;
                }
                if (button_interrupt_flag) continue;
            }

        

            /* Turn off all LEDs first */
            for (int i = 0; i < 5; i++) {
                GPIO_ClearPin(led_ports[i], led_pins[i]);
            }

            /* Light up current LED */
            uint8_t led_idx = circle_forward ? circle_step : (4 - circle_step);
            GPIO_SetPin(led_ports[led_idx], led_pins[led_idx]);

            /* Wait with LED on - using accelerated delay */
            for (uint32_t t = 0; t < base_delay / 10; ++t) {
                char c;
                if (UART_ReceiveChar(&c)) {
                    CLI_ProcessChar(c);
                }
                if (button_interrupt_flag) break;
                delay_us(10000);
            }
            if (button_interrupt_flag) continue;

            /* Advance to next LED */
            circle_step = (circle_step + 1) % 5;

            /* Check if we've completed one full cycle */
            if (circle_step == 0) {
                direction_counter++;
                /* Reverse direction after STEPS_PER_DIRECTION cycles */
                if (direction_counter >= STEPS_PER_DIRECTION) {
                    circle_forward = !circle_forward;
                    direction_counter = 0;
                    speed_step = 0; /* Reset speed on direction change */
                    /* Small pause when changing direction with quick flash */
                    for (int i = 0; i < 5; i++) {
                        GPIO_SetPin(led_ports[i], led_pins[i]);
                    }
                    for (uint32_t t = 0; t < 8; ++t) {
                        char c;
                        if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                        if (button_interrupt_flag) break;
                        delay_us(10000);
                    }
                    for (int i = 0; i < 5; i++) {
                        GPIO_ClearPin(led_ports[i], led_pins[i]);
                    }
                    for (uint32_t t = 0; t < 20; ++t) {
                        char c;
                        if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                        if (button_interrupt_flag) break;
                        delay_us(10000);
                    }
                    if (button_interrupt_flag) continue;
                }
            }

            } else if (led_auto_mode == 6) { /* DISCO mode - colorful rapid random patterns */
                /* Several short randomized patterns to give a disco/party effect */
                uint32_t pattern = lcg_rand();
                uint32_t ptype = pattern % 4;

                if (ptype == 0) {
                    /* Chase across LEDs forwards and backwards */
                    for (int dir = 0; dir < 2; dir++) {
                        if (button_interrupt_flag || led_auto_mode != 6) break;
                        for (int s = 0; s < 5; s++) {
                            int idx = dir == 0 ? s : (4 - s);
                            /* light one LED */
                            for (int i = 0; i < 5; i++) GPIO_ClearPin(led_ports[i], led_pins[i]);
                            GPIO_SetPin(led_ports[idx], led_pins[idx]);
                            for (uint32_t t = 0; t < 4; ++t) {
                                char c;
                                if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                                if (button_interrupt_flag || led_auto_mode != 6) break;
                                delay_us(20000); /* 20ms */
                            }
                            if (button_interrupt_flag || led_auto_mode != 6) break;
                        }
                    }
                } else if (ptype == 1) {
                    /* Random bursts: randomly turn on/off subsets */
                    uint32_t bursts = 2 + (lcg_rand() % 4);
                    for (uint32_t b = 0; b < bursts; ++b) {
                        if (button_interrupt_flag || led_auto_mode != 6) break;
                        for (int i = 0; i < 5; ++i) {
                            if ((lcg_rand() & 1) == 0) GPIO_SetPin(led_ports[i], led_pins[i]);
                            else GPIO_ClearPin(led_ports[i], led_pins[i]);
                        }
                        uint32_t on_ms = 30 + (lcg_rand() % 121); /* 30..150ms */
                        uint32_t loops = on_ms / 25;
                        for (uint32_t t = 0; t < loops; ++t) {
                            char c;
                            if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                            if (button_interrupt_flag || led_auto_mode != 6) break;
                            delay_us(25000);
                        }
                        for (int i = 0; i < 5; ++i) GPIO_ClearPin(led_ports[i], led_pins[i]);
                    }
                } else if (ptype == 2) {
                    /* Pulsate all LEDs together */
                    for (int rep = 0; rep < 3; ++rep) {
                        if (button_interrupt_flag || led_auto_mode != 6) break;
                        for (int i = 0; i < 5; ++i) GPIO_SetPin(led_ports[i], led_pins[i]);
                        for (uint32_t t = 0; t < 6; ++t) {
                            char c;
                            if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                            if (button_interrupt_flag || led_auto_mode != 6) break;
                            delay_us(25000);
                        }
                        for (int i = 0; i < 5; ++i) GPIO_ClearPin(led_ports[i], led_pins[i]);
                        for (uint32_t t = 0; t < 3; ++t) {
                            char c;
                            if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                            if (button_interrupt_flag || led_auto_mode != 6) break;
                            delay_us(25000);
                        }
                    }
                } else {
                    /* Alternating pairs */
                    for (int rep = 0; rep < 6; ++rep) {
                        if (button_interrupt_flag || led_auto_mode != 6) break;
                        /* pair A: 0+2, pair B:1+3, then 4 alone */
                        GPIO_ClearPin(led_ports[0], led_pins[0]);
                        GPIO_ClearPin(led_ports[1], led_pins[1]);
                        GPIO_ClearPin(led_ports[2], led_pins[2]);
                        GPIO_ClearPin(led_ports[3], led_pins[3]);
                        GPIO_ClearPin(led_ports[4], led_pins[4]);
                        GPIO_SetPin(led_ports[0], led_pins[0]);
                        GPIO_SetPin(led_ports[2], led_pins[2]);
                        for (uint32_t t = 0; t < 3; ++t) {
                            char c;
                            if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                            if (button_interrupt_flag || led_auto_mode != 6) break;
                            delay_us(20000);
                        }
                        GPIO_ClearPin(led_ports[0], led_pins[0]);
                        GPIO_ClearPin(led_ports[2], led_pins[2]);
                        GPIO_SetPin(led_ports[1], led_pins[1]);
                        GPIO_SetPin(led_ports[3], led_pins[3]);
                        for (uint32_t t = 0; t < 3; ++t) {
                            char c;
                            if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                            if (button_interrupt_flag || led_auto_mode != 6) break;
                            delay_us(20000);
                        }
                        GPIO_ClearPin(led_ports[1], led_pins[1]);
                        GPIO_ClearPin(led_ports[3], led_pins[3]);
                        GPIO_SetPin(led_ports[4], led_pins[4]);
                        for (uint32_t t = 0; t < 2; ++t) {
                            char c;
                            if (UART_ReceiveChar(&c)) CLI_ProcessChar(c);
                            if (button_interrupt_flag || led_auto_mode != 6) break;
                            delay_us(20000);
                        }
                        GPIO_ClearPin(led_ports[4], led_pins[4]);
                    }
                }
                if (button_interrupt_flag) continue;
        } else { /* OFF mode */
            /* Just check for UART input */
            char c;
            if (UART_ReceiveChar(&c)) {
                CLI_ProcessChar(c);
            }
            delay_us(50000); /* 50ms delay to remain responsive */
        }
        
        /* Handle CLI-controlled LED blinking (if any active) */
        uint32_t current_time = micros();
        for (int i = 0; i < 5; i++) {
            if (led_blinking[i] && (current_time - led_blink_times[i] >= 500000)) {
                GPIO_TogglePin(led_ports[i], led_pins[i]);
                led_blink_times[i] = current_time;
            }
        }
        
        /* Handle debug LED blinking */
        if (debug_led_blinking && (current_time - debug_led_blink_time >= 500000)) {
            LED_Toggle();
            debug_led_blink_time = current_time;
        }
    }
    
    return 0;
}

/**
  * @brief EXTI Line 2 and 3 Interrupt Handler
  *        Handles button press on PA2 (EXTI2)
  */
void EXTI2_3_IRQHandler(void)
{
    /* Check if EXTI line 2 caused the interrupt (Falling edge) */
    if (EXTI->FPR1 & (1UL << 2)) {
        /* Clear the pending bit by writing 1 to it */
        EXTI->FPR1 = (1UL << 2);
        /* Set the flag for the main loop */
        button_interrupt_flag = 1;
    }
    /* Also check Rising edge just in case */
    if (EXTI->RPR1 & (1UL << 2)) {
        EXTI->RPR1 = (1UL << 2);
    }
}
