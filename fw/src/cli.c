/* Command Line Interface */

#include "cli.h"
#include "uart.h"
#include "led.h"
#include "gpio.h"
#include "config.h"
#include "timer.h"
#include "i2c_eeprom.h"
#include "pins.h"
#include "stm32c0xx.h"
#include "core_cm0plus.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

/* External LED blinking variables from main.c */
extern bool led_blinking[5];
extern uint32_t led_blink_times[5];

#define FIRMWARE_VERSION "1.5.0-base"
#define SYSTEM_HOSTNAME "SRAL-SAO2"

/* EEPROM layout constants */
#define SAO_MAGIC_LIFE 0x4546494C  /* 'L' 'I' 'F' 'E' little-endian */
#define MARKER_OFF 0x36
#define MARKER_LEN 10
#define FIRMWARE_AREA_START 0x40
#define CALLSIGN_OFFSET (FIRMWARE_AREA_START)
#define CALLSIGN_SLOT_LEN 14  /* 13 chars + NUL */
/* CW message storage next to callsign: allow up to 20 visible chars + NUL */
#define CW_SLOT_OFFSET (CALLSIGN_OFFSET + CALLSIGN_SLOT_LEN)
#define CW_SLOT_LEN 21 /* 20 chars + NUL */

/* Default SAO binary descriptor (54 bytes) to write when EEPROM is corrupted.
    Format per Badge.Team binary_descriptor spec used by SRAL-SAO2.
    This array covers offsets 0x00..0x35. [[MARKER]] is written separately at 0x36..0x3F. */
static const uint8_t default_sao[54] = {
    /* 0x00..0x03: "LIFE" */
    'L','I','F','E',
    /* 0x04: name length */
    9,
    /* 0x05: driver name length */
    5,
    /* 0x06: driver data length */
    32,
    /* 0x07: extra drivers */
    0,
    /* 0x08..0x10: name "SRAL-SAO2" (9 bytes) */
    'S','R','A','L','-','S','A','O','2',
    /* 0x11..0x15: driver name "sral2" (5 bytes) */
    's','r','a','l','2',
    /* 0x16..0x35: driver data (32 bytes) - default pattern 0x00..0x1F */
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F
};

/* Default callsign to use when initializing EEPROM */
#define DEFAULT_CALLSIGN "wheel"

/* Persistent storage for configuration */
static char current_callsign[CALLSIGN_SLOT_LEN] = "wheel";  /* Default callsign/nick */
/* Persistent CW message to be sent via Morse (max 20 chars + NUL)
    Stored in RAM and persisted to EEPROM. Default is "SRAL". */
char current_cw[CW_SLOT_LEN] = "SRAL";

/* Helper: initialize the EEPROM to the firmware defaults.
    This rewrites the SAO header (0x00..0x35), [[MARKER]] (0x36..0x3F),
   zeroes the firmware area (0x40..0xFF) and writes the default callsign into
   the reserved callsign slot. */
static void EEPROM_InitializeDefaults(void) {
    UART_SendString("EEPROM reset to defaults\r\n");

    /* Write default SAO header */
    for (int i = 0; i < (int)sizeof(default_sao); i++) {
        eeprom_write_byte(i, default_sao[i]);
        delay_us(5000);
    }

    /* Write [[MARKER]] marker */
    const char otp[] = "[[MARKER]]";
    for (int i = 0; i < MARKER_LEN; i++) {
        eeprom_write_byte(MARKER_OFF + i, (uint8_t)otp[i]);
        delay_us(5000);
    }

    /* Zero firmware internal data area (0x40..0xFF) */
    for (uint16_t a = FIRMWARE_AREA_START; a <= 0xFF; a++) {
        eeprom_write_byte(a, 0x00);
        delay_us(5000);
    }

    /* Write default callsign into reserved slot */
    strncpy(current_callsign, DEFAULT_CALLSIGN, CALLSIGN_SLOT_LEN);
    current_callsign[CALLSIGN_SLOT_LEN - 1] = '\0';
    for (size_t i = 0; i < CALLSIGN_SLOT_LEN; i++) {
        uint8_t b = (i < strlen(current_callsign)) ? (uint8_t)current_callsign[i] : 0;
        eeprom_write_byte(CALLSIGN_OFFSET + i, b);
        delay_us(5000);
        if (b == 0) break;
    }

    /* Initialize CW slot to default "SRAL" and write to EEPROM */
    strncpy(current_cw, "SRAL", CW_SLOT_LEN);
    current_cw[CW_SLOT_LEN - 1] = '\0';
    for (size_t i = 0; i < CW_SLOT_LEN; i++) {
        uint8_t b = (i < strlen(current_cw)) ? (uint8_t)current_cw[i] : 0;
        eeprom_write_byte(CW_SLOT_OFFSET + (uint16_t)i, b);
        delay_us(5000);
        if (b == 0) break;
    }
}

static char cli_buffer[CLI_BUFFER_SIZE];
static uint32_t cli_index = 0;

/* Boot time for uptime calculation */
static uint32_t boot_time_us = 0;

/* Reset confirmation */
static bool awaiting_reset_confirmation = false;

/* Suppress prompt after command */
static bool suppress_prompt_after_command = false;

/* Forward declarations */
static void CLI_ParseCommand(const char *cmd);
static void CLI_Help(void);
static void CLI_StartLedBlink(int led_num);

/* Flash storage functions */
static void CLI_LoadConfig(void);
static void CLI_SaveConfig(void);
static bool CLI_ValidateCallsign(const char *callsign);

/* Flash storage functions */

/* Hint system */

/* Helper function to convert uint32_t to string */
static void uint32_to_str(uint32_t num, char *str, uint8_t max_len) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    uint8_t i = 0;
    uint32_t temp = num;
    while (temp > 0 && i < max_len - 1) {
        str[i++] = '0' + (temp % 10);
        temp /= 10;
    }

    str[i] = '\0';

    // Reverse the string
    uint8_t start = 0;
    uint8_t end = i - 1;
    while (start < end) {
        char temp_char = str[start];
        str[start] = str[end];
        str[end] = temp_char;
        start++;
        end--;
    }
}

void CLI_SetBootTime(void) {
    boot_time_us = micros();
}

/* Display uptime in a formatted way */
static void CLI_DisplayUptime(void) {
    uint32_t current_time = micros();
    uint32_t uptime_us = current_time - boot_time_us;
    uint32_t uptime_s = uptime_us / 1000000;
    uint32_t uptime_ms = (uptime_us % 1000000) / 1000;
    
    // Calculate days, hours, minutes, seconds
    uint32_t days = uptime_s / 86400;
    uint32_t hours = (uptime_s % 86400) / 3600;
    uint32_t minutes = (uptime_s % 3600) / 60;
    uint32_t seconds = uptime_s % 60;
    
    // Display days if any
    if (days > 0) {
        char str[16];
        uint32_to_str(days, str, sizeof(str));
        UART_SendString(str);
        UART_SendString(" days, ");
    }
    
    // Display hours if any or if days were shown
    if (hours > 0 || days > 0) {
        char str[16];
        uint32_to_str(hours, str, sizeof(str));
        UART_SendString(str);
        UART_SendString(" hours, ");
    }
    
    // Display minutes if any or if hours/days were shown
    if (minutes > 0 || hours > 0 || days > 0) {
        char str[16];
        uint32_to_str(minutes, str, sizeof(str));
        UART_SendString(str);
        UART_SendString(" minutes, ");
    }
    
    // Always show seconds
    char str[16];
    uint32_to_str(seconds, str, sizeof(str));
    UART_SendString(str);
    UART_SendString(" seconds, ");
    
    // Show milliseconds
    uint32_to_str(uptime_ms, str, sizeof(str));
    UART_SendString(str);
    UART_SendString(" ms\r\n");
}

void CLI_Init(void) {
    cli_index = 0;
    memset(cli_buffer, 0, sizeof(cli_buffer));
    CLI_LoadConfig();  // Load saved configuration
}

void CLI_PrintPrompt(void) {
    UART_SendString("\r\n");
    UART_SendString(current_callsign);
    UART_SendString("@");
    UART_SendString(SYSTEM_HOSTNAME);
    UART_SendString(":~> ");
}

void CLI_ProcessChar(char c) {
    if (c == '\r' || c == '\n') {
        /* End of command */
        cli_buffer[cli_index] = '\0';
        UART_SendString("\r\n");

        {
            if (cli_index > 0) {
                // Trim trailing whitespace from command
                char *end = cli_buffer + strlen(cli_buffer) - 1;
                while (end >= cli_buffer && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
                    *end = '\0';
                    end--;
                }
                CLI_ParseCommand(cli_buffer);
            }
        }

        cli_index = 0;
        /* Only show prompt if not suppressing */
        if (!suppress_prompt_after_command) {
            CLI_PrintPrompt();
        }
        suppress_prompt_after_command = false;
    }
    else if (c == '\b' || c == 0x7F) {
        /* Backspace */
        if (cli_index > 0) {
            cli_index--;
            UART_SendString("\b \b");
        }
    }
    else if (c >= 32 && c < 127) {
        /* Printable character */
        if (cli_index < CLI_BUFFER_SIZE - 1) {
            cli_buffer[cli_index++] = c;
            UART_SendChar(c);  /* Echo */
        }
    }
}

static void CLI_LoadConfig(void) {

    // For SRAL-SAO2, read from I2C EEPROM
    // Check for SAO magic 'LIFE' at address 0x00-0x03
    uint32_t magic = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t byte;
        if (eeprom_read_byte(i, &byte) != 0) {
            return;  // EEPROM read error, keep defaults
        }
        magic |= ((uint32_t)byte << (i * 8));
    }

    // Check for [[MARKER]] marker at 0x36
    char otp_buf[MARKER_LEN + 1];
    for (int i = 0; i < MARKER_LEN; i++) {
        uint8_t b;
        if (eeprom_read_byte(MARKER_OFF + i, &b) != 0) {
            otp_buf[i] = '\0';
            break;
        }
        otp_buf[i] = (char)b;
    }
    otp_buf[MARKER_LEN] = '\0';

    bool life_ok = (magic == SAO_MAGIC_LIFE);
    bool otp_ok = (strncmp(otp_buf, "[[MARKER]]", MARKER_LEN) == 0);

    if (!life_ok || !otp_ok) {
        EEPROM_InitializeDefaults();
    }

    // Read callsign from firmware area (fixed slot at CALLSIGN_OFFSET)
    for (size_t i = 0; i < CALLSIGN_SLOT_LEN; i++) {
        uint8_t byte;
        if (eeprom_read_byte(CALLSIGN_OFFSET + (uint16_t)i, &byte) != 0) {
            return; // EEPROM read error
        }
        current_callsign[i] = (char)byte;
        if (byte == 0) break;
    }
    current_callsign[CALLSIGN_SLOT_LEN - 1] = '\0';

    /* Read CW message from EEPROM */
    for (size_t i = 0; i < CW_SLOT_LEN; i++) {
        uint8_t byte;
        if (eeprom_read_byte(CW_SLOT_OFFSET + (uint16_t)i, &byte) != 0) {
            return; // EEPROM read error
        }
        current_cw[i] = (char)byte;
        if (byte == 0) break;
    }
    current_cw[CW_SLOT_LEN - 1] = '\0';

}

static void CLI_SaveConfig(void) {
//    UART_SendString("Saving cfg to EEPROM...\r\n");

    // Check [[MARKER]] marker integrity
    char otp_buf[MARKER_LEN + 1];
    bool otp_valid = true;
    for (int i = 0; i < MARKER_LEN; i++) {
        uint8_t b;
        if (eeprom_read_byte(MARKER_OFF + i, &b) != 0) {
            otp_valid = false;
            break;
        }
        otp_buf[i] = (char)b;
    }
    otp_buf[MARKER_LEN] = '\0';
    
    if (!otp_valid || strncmp(otp_buf, "[[MARKER]]", MARKER_LEN) != 0) {
        UART_SendString("EEPROM corrupted, say 'reset'\r\n");
        return;
    }

    // Write callsign to firmware area slot (14 bytes)
    for (size_t i = 0; i < CALLSIGN_SLOT_LEN; i++) {
        uint8_t b = (i < strlen(current_callsign)) ? (uint8_t)current_callsign[i] : 0;
        if (eeprom_write_byte(CALLSIGN_OFFSET + (uint16_t)i, b) != 0) {
            UART_SendString("Err: Failed to save callsign.\r\n");
            return;
        }
        delay_us(5000);
        if (b == 0) break;
    }

    // Write CW message to firmware area slot (21 bytes)
    for (size_t i = 0; i < CW_SLOT_LEN; i++) {
        uint8_t b = (i < strlen(current_cw)) ? (uint8_t)current_cw[i] : 0;
        if (eeprom_write_byte(CW_SLOT_OFFSET + (uint16_t)i, b) != 0) {
            UART_SendString("Err: Failed to save CW msg.\r\n");
            return;
        }
        delay_us(5000);
        if (b == 0) break;
    }
    UART_SendString("Saved\r\n");
}

static bool CLI_ValidateCallsign(const char *callsign) {
    if (strlen(callsign) == 0 || strlen(callsign) > 12) {
        return false;
    }
    
    for (size_t i = 0; i < strlen(callsign); i++) {
        char c = callsign[i];
        if (!isalnum(c) && c != '-' && c != '/') {
            return false;
        }
    }
    return true;
}

/* Validate a CW message: printable ASCII characters, length 1..20 */
static bool CLI_ValidateCW(const char *msg) {
    size_t len = strlen(msg);
    if (len == 0 || len > (size_t)(CW_SLOT_LEN - 1)) return false;
    for (size_t i = 0; i < len; ++i) {
        char c = msg[i];
        if (c == ' ') continue; /* space allowed as word separator */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) continue;
        if (c >= '0' && c <= '9') continue;
        return false; /* other characters not allowed */
    }
    return true;
}

static void CLI_ParseCommand(const char *cmd) {
    if (awaiting_reset_confirmation) {
        if (strcmp(cmd, "y") == 0 || strcmp(cmd, "Y") == 0) {
            // Reset in persistent storage by reinitializing EEPROM to defaults
            EEPROM_InitializeDefaults();
        } else {
            UART_SendString("Cancelled\r\n");
        }
        awaiting_reset_confirmation = false;
        return;
    }

    if (strcmp(cmd, "help") == 0) {
        CLI_Help();
    }
    else if (strcmp(cmd, "ver") == 0 || strcmp(cmd, "version") == 0) {
        UART_SendString("SRAL-SAO2 v");
        UART_SendString(FIRMWARE_VERSION);
        UART_SendString("\r\n");
    }
    else if (strcmp(cmd, "pwr") == 0) {
        /* Check BADGE_PWR_SENSE pin to determine power source */
        uint8_t pin_state = GPIO_ReadPin(BADGE_PWR_SENSE_GPIO_PORT, BADGE_PWR_SENSE_GPIO_PIN);
        if (pin_state) {
            UART_SendString("PWR: Badge; SAO IDC\r\n");
        } else {
            UART_SendString("PWR: Battery/SWD\r\n");
        }
    }
    else if (strcmp(cmd, "reset") == 0) {
        UART_SendString("Defaults, really? y/N: ");
        awaiting_reset_confirmation = true;
        suppress_prompt_after_command = true;
    }
    else if (strcmp(cmd, "reboot") == 0 || strcmp(cmd, "restart") == 0) {
        UART_SendString("Rebooting..\r\n");
        // Small delay to let UART finish transmitting
        for (volatile int i = 0; i < 100000; i++);
        // Trigger system reset using CMSIS function
        NVIC_SystemReset();
        // Should not reach here
        while (1);
    }
    else if (strncmp(cmd, "setcall ", 8) == 0 || strncmp(cmd, "setnick ", 8) == 0) {
        const char *callsign = cmd + 8;
        if (CLI_ValidateCallsign(callsign)) {
            strcpy(current_callsign, callsign);
            CLI_SaveConfig();
            UART_SendString("Callsign/nick set to: ");
            UART_SendString(current_callsign);
            UART_SendString("\r\n");
        } else {
            UART_SendString("Invalid callsign/nick. A-Z/a-z,0-9,'-','/' only (1-12 chars)\r\n");
        }
    }
    else if (strcmp(cmd, "callsign") == 0 || strcmp(cmd, "whoami") == 0) {
        UART_SendString(current_callsign);
        UART_SendString("\r\n");
    }
    else if (strcmp(cmd, "who") == 0) {
        UART_SendString(current_callsign);
        UART_SendString("\tttyS0\r\n");
    }
    else if (strcmp(cmd, "dmesg") == 0) {
        CLI_ShowBootMessages(false);
    }
    else if (strcmp(cmd, "led on") == 0) {
        extern bool debug_led_blinking;
        debug_led_blinking = false;
        LED_SetMode(LED_MODE_ON);
        UART_SendString("LED ON\r\n");
    }
    else if (strcmp(cmd, "led off") == 0) {
        extern bool debug_led_blinking;
        debug_led_blinking = false;
        LED_SetMode(LED_MODE_OFF);
        UART_SendString("LED OFF\r\n");
    }
    else if (strcmp(cmd, "led blink") == 0) {
        extern bool debug_led_blinking;
        extern uint32_t debug_led_blink_time;
        debug_led_blinking = true;
        debug_led_blink_time = micros();
        LED_SetMode(LED_MODE_ON);
        UART_SendString("LED blink\r\n");
    }
    else if (strcmp(cmd, "blinkmode") == 0 || strcmp(cmd, "automode") == 0 || strcmp(cmd, "bm") == 0) {
    extern volatile uint8_t led_auto_mode;
        UART_SendString("Auto-blink mode: ");
        if (led_auto_mode < 9) {
            UART_SendString(led_blink_mode_names[led_auto_mode]);
            char buf[16];
            UART_SendString(" (");
            uint32_to_str(led_auto_mode, buf, sizeof(buf));
            UART_SendString(buf);
            UART_SendString(")");
        } else {
            char buf[16];
            UART_SendString("UNKNOWN (");
            uint32_to_str(led_auto_mode, buf, sizeof(buf));
            UART_SendString(buf);
            UART_SendString(")");
        }
        UART_SendString("\r\n");
        UART_SendString("Use button or 'bm <0-6>' to change\r\n");
    }
    else if (strncmp(cmd, "blinkmode ", 10) == 0 || strncmp(cmd, "automode ", 9) == 0 || strncmp(cmd, "bm ", 3) == 0) {
        extern volatile uint8_t led_auto_mode;
        const char *param;
        if (strncmp(cmd, "blinkmode ", 10) == 0) {
            param = cmd + 10;
        } else if (strncmp(cmd, "bm ", 3) == 0) {
            param = cmd + 3;
        } else {
            param = cmd + 9;
        }
        int mode = atoi(param);
        int max_mode = 6;
        
        if (mode >= 0 && mode <= max_mode) {
            led_auto_mode = mode;
            UART_SendString("Auto-blink mode set to: ");
            UART_SendString(led_blink_mode_names[led_auto_mode]);
            UART_SendString("\r\n");
            /* Clear all LEDs when changing modes */
            for (int i = 1; i <= 5; i++) {
                PWM_SetDutyCycle(i, 0);
            }
            /* Turn off all LEDs when entering OFF mode */
            if (led_auto_mode == 0) {
                extern bool led_blinking[5];
                for (int i = 0; i < 5; i++) {
                    led_blinking[i] = false;
                }
            }
        } else {
            UART_SendString("Invalid mode. Use 0-6 (");
            for (int i = 0; i <= 6; i++) {
            if (i > 0) UART_SendString("/");
            UART_SendString(led_blink_mode_names[i]);
            }
            UART_SendString(")\r\n");}
    }
    else if (strncmp(cmd, "bled ", 5) == 0) {
        const char *param = cmd + 5;
        if (strcmp(param, "off") == 0 || strcmp(param, "stop") == 0) {
            // Stop all LED blinking
            for (int i = 0; i < 5; i++) {
                led_blinking[i] = false;
                PWM_SetDutyCycle(i + 1, 0);
            }
            UART_SendString("All LEDs off\r\n");
        } else {
            // Parse LED number
            int led_num = 0;
            if (param[0] >= '1' && param[0] <= '5' && param[1] == '\0') {
                led_num = param[0] - '0';
                // Start blinking the specified LED
                CLI_StartLedBlink(led_num);
                UART_SendString("LED");
                UART_SendChar('0' + led_num);
                if (led_num <= 4) {
                    UART_SendString(" blink\r\n");
                } else {
                    UART_SendString(" blink\r\n");
                }
            } else {
                UART_SendString("Usage: bled <1-5> or bled off/stop\r\n");
            }
        }
    }
    else if (strcmp(cmd, "status") == 0) {
        UART_SendString("System Status:\r\n");
        UART_SendString("  Board: SRAL-SAO2 (6 KB RAM / 32 KB flash, 256 B EEPROM)\r\n");
        UART_SendString("  Clock: 12 MHz\r\n");
        UART_SendString("  OS: System AX.25/OS; SAO edition\r\n");
        UART_SendString("  FW: v");
        UART_SendString(FIRMWARE_VERSION);
        UART_SendString("\r\n  ttyS0: 115200 8N1\r\n");
        UART_SendString("\r\n");
        
        // Add uptime information
        UART_SendString("  Uptime: ");
        CLI_DisplayUptime();
    }
    else if (strcmp(cmd, "uptime") == 0) {
        UART_SendString("Uptime: ");
        CLI_DisplayUptime();
    }
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "logout") == 0) {
        // Hidden command: exit/logout - Inception reference
        UART_SendString("Haven't seen Inception? Be careful out there\r\n");
    }
    else if (strcmp(cmd, "ls") == 0 || strncmp(cmd, "ls ", 3) == 0) {
        /* List "files". Accepts "ls" or "ls <file>"; trim params. */
        const char *arg = NULL;
        if (strncmp(cmd, "ls ", 3) == 0) arg = cmd + 3;
        if (arg) {
            while (*arg == ' ') arg++; /* skip extra spaces */
        }
        /* Only one virtual file exists: README */
        if (arg && *arg && strcasecmp(arg, "README") != 0) {
            UART_SendString("No such file\r\n");
        } else {
            UART_SendString("README\r\n");
        }
    }
    else if (strcmp(cmd, "hostname") == 0) {
        UART_SendString(SYSTEM_HOSTNAME);
        UART_SendString("\r\n");
    }
    else if (strncmp(cmd, "cat ", 4) == 0) {
        const char *filename = cmd + 4;
        /* Skip any number of leading spaces */
        while (*filename == ' ') filename++;
        if (*filename == '\0') {
            UART_SendString("Usage: cat <filename>\r\n");
        } else if (strcasecmp(filename, "README") == 0) {
            UART_SendString("Base FW by OH3HZB. Enjoy SRAL-SAO2!\r\nSRAL: https://www.sral.fi\r\n");
        } else {
            UART_SendString("cat: ");
            UART_SendString(filename);
            UART_SendString(": No such file or directory\r\n");
        }
    }
    else if (strcmp(cmd, "cat") == 0) {
        UART_SendString("Usage: cat <filename>\r\n");   
    }
    else if (strncmp(cmd, "cw ", 3) == 0) {
        /* Set CW message (max 13 printable chars) */
        const char *msg = cmd + 3;
        while (*msg == ' ') msg++; /* skip extra spaces */
        if (!CLI_ValidateCW(msg)) {
            UART_SendString("Invalid CW message. Use 1-20 chars: A-Z, 0-9 and space only.\r\n");
        } else {
            strncpy(current_cw, msg, CW_SLOT_LEN);
            /* ensure NUL termination */
            current_cw[CW_SLOT_LEN - 1] = '\0';
              /* Persist to EEPROM */
              CLI_SaveConfig();
            UART_SendString("CW msg set: ");
            UART_SendString(current_cw);
            UART_SendString("\r\n");
        }
    }
    else if (strcmp(cmd, "cw") == 0) {
        UART_SendString("Current CW msg: ");
        if (current_cw[0]) UART_SendString(current_cw);
        else UART_SendString("(none)");
        UART_SendString("\r\n");
    }
    /* 'eetest' command removed: EEPROM quick-test not needed */
    else if (strncmp(cmd, "eeread ", 7) == 0) {
        uint8_t data;
        uint16_t addr = atoi(cmd + 7);
        if (eeprom_read_byte(addr, &data) == 0) {
            char buf[8];
            uint32_to_str(data, buf, sizeof(buf));
            UART_SendString(buf);
            UART_SendString("\r\n");
        } else {
            UART_SendString("FAIL\r\n");
        }
    }
    else if (strncmp(cmd, "eewrite ", 8) == 0) {
        const char *args = cmd + 8;
        const char *space = strchr(args, ' ');
        if (space) {
            uint16_t addr = atoi(args);
            uint8_t data = atoi(space + 1);
            if (eeprom_write_byte(addr, data) == 0) {
                UART_SendString("OK\r\n");
                delay_us(5000);
            } else {
                UART_SendString("FAIL\r\n");
            }
        }
    }
    else if (strlen(cmd) > 0) {
        UART_SendString("Unknown cmd: ");
        UART_SendString(cmd);
        UART_SendString("\r\nType 'help' for help\r\n");
    }
}

static void CLI_Help(void) {
    UART_SendString("Available commands:\r\n");
    UART_SendString("  ver/version        - Firmware version\r\n");
    UART_SendString("  pwr                - Power source\r\n");
    UART_SendString("  led on/off/blink   - Debug LED ctrl\r\n");
    UART_SendString("  bled <1-5>/off     - Blink badge LED (bled off/stop to stop)\r\n");
    UART_SendString("  bm/blinkmode [0-6] - Get/set auto-blink mode (0=OFF,1=BLINK,2=FADE,3=CW,4=STROBO,5=ICIRCLE,6=DISCO)\r\n");
    UART_SendString("  status             - System status\r\n");
    UART_SendString("  uptime             - Show system uptime\r\n");
    UART_SendString("  ls                 - List files\r\n");
    UART_SendString("  cat <file>         - Show file\r\n");
    UART_SendString("  cw <msg>           - Set/show CW message (1-20 chars)\r\n");
    UART_SendString("  reset              - Factory reset\r\n");
    UART_SendString("  setcall/setnick <c>- Set callsign/nickname\r\n");
    UART_SendString("  who                - Show users\r\n");
    UART_SendString("  dmesg              - Show boot messages\r\n");
    UART_SendString("  eeread <addr>      - Read byte from EEPROM addr\r\n");
    UART_SendString("  eewrite <addr> <d> - Write byte to EEPROM addr\r\n");
    UART_SendString("  reboot             - Reboot\r\n\r\n");
}

void CLI_ShowBootMessages(bool with_delays) {
    static const char * const msgs[] = {
        "\r\n\r\nSRAL-SAO2 fw v", FIRMWARE_VERSION, " booting...\r\n\r\n",
        "CPU clock 12 MHz... OK\r\n",
        "\r\nSystem ready. Type 'help' for help\r\n"
    };
    static const uint8_t delays[] = {100,50,75,50,50,100,75,75,75,100,75,50,50,50,50,50,50,50,0};
    
    for (uint8_t i = 0; i < sizeof(msgs)/sizeof(msgs[0]); i++) {
        UART_SendString(msgs[i]);
        if (with_delays && delays[i]) delay_ms(delays[i]);
    }
    
    /* Indicate if SAO UART was enabled at boot */
    extern bool uart2_enabled;
    if (uart2_enabled) {
        UART_SendString("SAO_IDC UART enabled (pin5/gpio1: RX, pin6/gpio2: TX)\r\n");
        if (with_delays) delay_ms(50);
    }
    else {
        UART_SendString("Hold BTN when powering on to enable SAO_IDC UART (pin5/gpio1: RX, pin6/gpio2: TX)\r\n");
        if (with_delays) delay_ms(50);
    }
}

static void CLI_StartLedBlink(int led_num) {
    if (led_num >= 1 && led_num <= 5) {
        led_blinking[led_num - 1] = true;
        led_blink_times[led_num - 1] = micros();
    }
}

