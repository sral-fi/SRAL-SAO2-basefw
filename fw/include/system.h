#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

/* System initialization */
void System_Init(void);

/* Clock configuration */
void System_ClockConfig(void);

/* Get system clock frequency */
uint32_t System_GetClock(void);

#endif /* SYSTEM_H */
