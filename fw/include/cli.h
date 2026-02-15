#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stdbool.h>

/* CLI initialization */
void CLI_Init(void);

/* CLI input processing */
void CLI_ProcessChar(char c);

/* CLI prompt */
void CLI_PrintPrompt(void);

/* Set boot time for uptime tracking */
void CLI_SetBootTime(void);

/* Show boot messages */
void CLI_ShowBootMessages(bool with_delays);
/* Expose CW buffer for LED blink mode */
extern char current_cw[21];

#endif /* CLI_H */
