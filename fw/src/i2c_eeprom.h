/* Simple bit-banged I2C driver for 24C02 EEPROM
 * Uses GPIO toggling on pins defined in pins.h (I2C_SCL_GPIO_PIN / I2C_SDA_GPIO_PIN)
 */
#ifndef I2C_EEPROM_H
#define I2C_EEPROM_H

#include <stdint.h>

void eeprom_init(void);
int eeprom_write_byte(uint16_t mem_addr, uint8_t data);
int eeprom_read_byte(uint16_t mem_addr, uint8_t *data);
uint32_t eeprom_get_last_isr(void);
uint32_t eeprom_get_cr2(void);
uint32_t eeprom_get_timing(void);
uint32_t eeprom_get_cr1(void);
/* internal pull-up control removed: external pull-ups always used */

#endif /* I2C_EEPROM_H */
