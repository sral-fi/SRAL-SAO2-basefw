#include "i2c_eeprom.h"
#include "pins.h"
#include "stm32c0xx.h"
#include "config.h"

/* Hardware I2C1 driver for 24C02 EEPROM (master mode)
 * - Uses I2C1 peripheral, pins from `pins.h` (PA9=SCL, PA10=SDA with SYSCFG remap applied)
 * - Internal pull-ups can be enabled/disabled via I2C_USE_INTERNAL_PULLUPS in pins.h
 * - Uses CR2/ISR/TXDR/RXDR register-based blocking transfers for single-byte read/write
 */

/* Default TIMING value. This came from STM32Cube-generated examples and is a reasonable
 * starting point for 100 kHz-ish operation. If you observe timing issues, tune this
 * value or compute it for your exact clock configuration.
 */
/* Default TIMING value fallback (kept for reference). We'll compute timing
   dynamically based on SystemCoreClock and a target I2C speed. */
#ifndef I2C_TIMING_DEFAULT
#define I2C_TIMING_DEFAULT 0x00303D5BUL
#endif

/* Compute a TIMINGR value for a desired I2C frequency (Hz).
   Strategy: iterate PRESC from 0..15 and compute total SCL period in
   peripheral clocks: period_clks = Fclk/(PRESC+1)/freq. Then SCLL+SCLH = period_clks - 2.
   Split SCLL and SCLH approximately half each, clamp to 0..255. Use modest
   SDADEL and SCLDEL values (small) to be conservative. Return TIMINGR packed
   as [PRESC(31:28) SCLDEL(27:24) SDADEL(23:20) SCLH(15:8) SCLL(7:0)].
*/
static uint32_t i2c_compute_timing(uint32_t pclk_hz, uint32_t i2c_hz)
{
    if (i2c_hz == 0 || pclk_hz == 0) return I2C_TIMING_DEFAULT;

    for (uint32_t presc = 0; presc <= 15; ++presc) {
        uint32_t presc_div = presc + 1;
        /* Use 64-bit to avoid overflow */
        uint64_t period_clks = (uint64_t)pclk_hz * 1ULL / (presc_div * i2c_hz);
        if (period_clks < 4) continue; /* need at least SCLL+SCLH+2 >= 4 */

        uint64_t total = period_clks - 2;
        if (total > 510) continue; /* SCLL+SCLH must fit into 0..510 */

        /* Split into SCLL and SCLH (prefer SCLL slightly longer) */
        uint32_t scll = (uint32_t)(total / 2 + (total % 2));
        uint32_t sclh = (uint32_t)(total - scll);
        if (scll > 255 || sclh > 255) continue;

        uint32_t scldel = 4; /* small delays (tunable) */
        uint32_t sdadel = 2;

        uint32_t timing = (presc << 28) | (scldel << 24) | (sdadel << 20) | (sclh << 8) | (scll);
        return timing;
    }

    /* Fallback */
    return I2C_TIMING_DEFAULT;
}

/* Helper: configure GPIOA pins for AF6 (I2C1), open-drain, no internal pull-ups */
static void i2c_gpio_init_hw(void)
{
    /* Ensure GPIOA clock enabled */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;

    /* Use physical pin definitions from pins.h when available; fall back to logical GPIO pins. */
     /* Use the physical pin numbers from `pins.h`. The header documents the
         physical mapping (e.g. logical PA9 may be physically PA11 on some packages).
         Rely on the definitions in `pins.h` so remap handling is centralized. */
     uint32_t scl_pin = I2C_SCL_GPIO_PIN;
     uint32_t sda_pin = I2C_SDA_GPIO_PIN;

#ifdef I2C_SCL_PHYSICAL_PIN
    scl_pin = I2C_SCL_PHYSICAL_PIN;
#endif
#ifdef I2C_SDA_PHYSICAL_PIN
    sda_pin = I2C_SDA_PHYSICAL_PIN;
#endif

    /* Configure SCL and SDA as Alternate Function (AF) */
    GPIOA->MODER &= ~(3UL << (scl_pin * 2));
    GPIOA->MODER |=  (2UL << (scl_pin * 2));
    GPIOA->MODER &= ~(3UL << (sda_pin * 2));
    GPIOA->MODER |=  (2UL << (sda_pin * 2));

    /* Set alternate function AF6 for pins 8..15 in AFR[1] */
    GPIOA->AFR[1] &= ~((0xFUL << ((scl_pin - 8) * 4)) | (0xFUL << ((sda_pin - 8) * 4)));
    GPIOA->AFR[1] |=  ((I2C_SCL_AF & 0xF) << ((scl_pin - 8) * 4)) | ((I2C_SDA_AF & 0xF) << ((sda_pin - 8) * 4));

    /* Configure output type open-drain */
    GPIOA->OTYPER |= (1UL << scl_pin) | (1UL << sda_pin);

    /* Configure internal pull-ups based on I2C_USE_INTERNAL_PULLUPS setting */
#if I2C_USE_INTERNAL_PULLUPS
    /* Enable internal pull-ups (PUPDR = 01) */
    GPIOA->PUPDR &= ~((3UL << (scl_pin * 2)) | (3UL << (sda_pin * 2)));
    GPIOA->PUPDR |=  ((1UL << (scl_pin * 2)) | (1UL << (sda_pin * 2)));
#else
    /* Disable internal pull-ups/pull-downs (PUPDR = 00).
       Board uses external 4.7k pull-ups on SCL/SDA. */
    GPIOA->PUPDR &= ~((3UL << (scl_pin * 2)) | (3UL << (sda_pin * 2)));
#endif

    /* Optionally set moderate speed */
    GPIOA->OSPEEDR &= ~((3UL << (scl_pin * 2)) | (3UL << (sda_pin * 2)));
    GPIOA->OSPEEDR |=  ((1UL << (scl_pin * 2)) | (1UL << (sda_pin * 2)));
}

/* Small delay used during bus recovery (tunable) */
static void i2c_short_delay(void)
{
    for (volatile int i = 0; i < 2000; ++i) {
        __asm__("nop");
    }
}

/* Attempt to free a stuck I2C bus by toggling SCL up to 9 times while monitoring SDA.
   Honors SYSCFG remap to toggle the physical SCL pin (PA9 or PA11). */
static void i2c_bus_recover_hw(void)
{
    /* Ensure GPIOA clock enabled */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;

    uint32_t scl_pin = I2C_SCL_GPIO_PIN;
    uint32_t sda_pin = I2C_SDA_GPIO_PIN;
#ifdef I2C_SCL_PHYSICAL_PIN
    scl_pin = I2C_SCL_PHYSICAL_PIN;
#endif
#ifdef I2C_SDA_PHYSICAL_PIN
    sda_pin = I2C_SDA_PHYSICAL_PIN;
#endif
    /* Configure SCL as general-purpose open-drain output, SDA as input (pull-up left to external)
       Save and modify only necessary registers (we keep it simple). */
    /* Set SCL output (01) */
    GPIOA->MODER &= ~(3UL << (scl_pin * 2));
    GPIOA->MODER |=  (1UL << (scl_pin * 2));
    /* Make SCL open-drain */
    GPIOA->OTYPER |= (1UL << scl_pin);
    /* Ensure SDA is input */
    GPIOA->MODER &= ~(3UL << (sda_pin * 2));

    /* Pulse SCL up to 9 times; if SDA goes high, bus released */
    for (int i = 0; i < 9; ++i) {
        /* Drive SCL high */
        GPIOA->BSRR = (1UL << scl_pin);
        i2c_short_delay();
        /* Read SDA; if high, bus released */
        if (GPIOA->IDR & (1UL << sda_pin)) break;
        /* Drive SCL low */
        GPIOA->BSRR = (1UL << (scl_pin + 16));
        i2c_short_delay();
    }

    /* Issue a STOP by driving SDA high while SCL high: ensure SCL high then set SDA as output high briefly */
    GPIOA->BSRR = (1UL << scl_pin);
    i2c_short_delay();
    /* Configure SDA as output open-drain and drive high */
    GPIOA->MODER &= ~(3UL << (sda_pin * 2));
    GPIOA->MODER |=  (1UL << (sda_pin * 2));
    GPIOA->BSRR = (1UL << sda_pin);
    i2c_short_delay();

    /* Restore SDA to input mode (external pull-ups remain) */
    GPIOA->MODER &= ~(3UL << (sda_pin * 2));
}

/* Wait helper with timeout (simple) */
static int wait_until_set(volatile uint32_t *reg, uint32_t mask, uint32_t timeout)
{
    while ((*(reg) & mask) == 0) {
        if (--timeout == 0) return -1;
    }
    return 0;
}

void eeprom_init(void)
{
    /* Ensure SYSCFG remap for PA11/PA12 if board uses remapped pins */
        /* The SYSCFG remap checks are removed as we are using the pin macros directly. */
        /* No need to enable SYSCFG clock or set remap bits. */

    /* Attempt bus recovery in case lines are stuck (clock held low by device) */
    i2c_bus_recover_hw();

    /* Configure GPIO pins for I2C hardware (AF6, open-drain, no internal pull-ups) */
    i2c_gpio_init_hw();

    /* external 4.7k pull-ups are present; internal pull-ups are left disabled */

    /* Enable I2C1 clock on APB */
    RCC->APBENR1 |= RCC_APBENR1_I2C1EN;

    /* Reset and release I2C1 to ensure clean state */
    RCC->APBRSTR1 |= RCC_APBRSTR1_I2C1RST;
    RCC->APBRSTR1 &= ~RCC_APBRSTR1_I2C1RST;

    /* Configure timing register for 20 kHz bus speed. Compute TIMINGR from
       the system clock so it's correct for the board's clock. */
    {
        extern uint32_t SystemCoreClock; /* from CMSIS system file */
        uint32_t timing = i2c_compute_timing(SystemCoreClock, 20000U);
        I2C1->TIMINGR = timing;
    }

    /* Enable peripheral */
    I2C1->CR1 |= I2C_CR1_PE;
}

/* Helper: send a master write of N bytes (data buffer provided) to 7-bit slave */
static int i2c_master_write(uint8_t dev7, const uint8_t *buf, uint8_t len)
{
    /* If bus is busy, wait for it to clear (timeouted) */
    uint32_t timeout = 100000;
    if (I2C1->ISR & I2C_ISR_BUSY) {
        while ((I2C1->ISR & I2C_ISR_BUSY) != 0) {
            if (--timeout == 0) return -1;
        }
    }

    /* Program CR2: SADD, NBYTES, AUTOEND, START */
    uint32_t cr2 = 0;
    /* CR2.SADD expects the 7-bit address left-aligned (address << 1) for 7-bit mode */
    cr2 |= ((uint32_t)((dev7 & 0x7F) << 1) << I2C_CR2_SADD_Pos);
    cr2 |= ((uint32_t)len << I2C_CR2_NBYTES_Pos);
    cr2 |= I2C_CR2_AUTOEND;
    /* write direction (RD_WRN = 0) */
    cr2 |= I2C_CR2_START;
    I2C1->CR2 = cr2;

    for (uint8_t i = 0; i < len; ++i) {
        /* Wait for TXIS (transmit interrupt status) */
        timeout = 100000;
        if (wait_until_set(&I2C1->ISR, I2C_ISR_TXIS, timeout) != 0) {
            /* check NACK */
            if (I2C1->ISR & I2C_ISR_NACKF) {
                I2C1->ICR = I2C_ICR_NACKCF;
            }
            return -1;
        }
        I2C1->TXDR = buf[i];
    }

    /* Wait for STOPF (transfer complete) */
    timeout = 100000;
    if (wait_until_set(&I2C1->ISR, I2C_ISR_STOPF, timeout) != 0) return -1;
    /* Clear STOP flag */
    I2C1->ICR = I2C_ICR_STOPCF;
    return 0;
}

/* Helper: master read N bytes into buf */
static int i2c_master_read(uint8_t dev7, uint8_t *buf, uint8_t len)
{
    /* Program CR2 for read: SADD, NBYTES, START, RD_WRN=1 */
    uint32_t cr2 = 0;
    /* CR2.SADD expects the 7-bit address left-aligned (address << 1) for 7-bit mode */
    cr2 |= ((uint32_t)((dev7 & 0x7F) << 1) << I2C_CR2_SADD_Pos);
    cr2 |= ((uint32_t)len << I2C_CR2_NBYTES_Pos);
    cr2 |= I2C_CR2_RD_WRN;
    cr2 |= I2C_CR2_AUTOEND;
    cr2 |= I2C_CR2_START;
    I2C1->CR2 = cr2;

    for (uint8_t i = 0; i < len; ++i) {
        /* Wait for RXNE */
        uint32_t timeout = 100000;
        if (wait_until_set(&I2C1->ISR, I2C_ISR_RXNE, timeout) != 0) return -1;
        buf[i] = (uint8_t)(I2C1->RXDR & 0xFF);
    }

    /* Wait for STOPF */
    uint32_t timeout = 100000;
    if (wait_until_set(&I2C1->ISR, I2C_ISR_STOPF, timeout) != 0) return -1;
    /* Clear STOP */
    I2C1->ICR = I2C_ICR_STOPCF;
    return 0;
}

static uint32_t i2c_last_isr = 0;

int eeprom_write_byte(uint16_t mem_addr, uint8_t data)
{
    uint8_t dev7 = (uint8_t)((EEPROM_I2C_ADDR >> 1) & 0x7F);
    uint8_t buf[2];
    buf[0] = (uint8_t)(mem_addr & 0xFF);
    buf[1] = data;

    int r = i2c_master_write(dev7, buf, 2);
    i2c_last_isr = I2C1->ISR;
    return r;
}

int eeprom_read_byte(uint16_t mem_addr, uint8_t *data)
{
    uint8_t dev7 = (uint8_t)((EEPROM_I2C_ADDR >> 1) & 0x7F);
    uint8_t addr = (uint8_t)(mem_addr & 0xFF);

    /* Write memory address (single byte) */
    if (i2c_master_write(dev7, &addr, 1) != 0) {
        i2c_last_isr = I2C1->ISR;
        return -1;
    }

    /* Read single byte */
    int r = i2c_master_read(dev7, data, 1);
    i2c_last_isr = I2C1->ISR;
    return r;
}

/* Expose last ISR for debugging */
uint32_t eeprom_get_last_isr(void)
{
    return i2c_last_isr;
}

uint32_t eeprom_get_cr2(void)
{
    return I2C1->CR2;
}

uint32_t eeprom_get_timing(void)
{
    return I2C1->TIMINGR;
}

uint32_t eeprom_get_cr1(void)
{
    return I2C1->CR1;
}

/* Internal pull-up toggling removed: board uses external pull-ups and
   internal pull-up control is not needed. */
