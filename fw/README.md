# SRAL-SAO2 badge firmware

This is the base firmware for the **SRAL-SAO2** (Simple/Sh*tty Add-On version 2), an interactive SAO hybrid badge for ham radio enthusiasts. The firmware runs on the STM32C011F6P6 microcontroller and includes interactive games, LED effects, and I2C EEPROM storage for persistent variables.

## Hardware Configuration

### Target MCU
- **Device**: STM32C011F6P6
- **Core**: ARM Cortex-M0+
- **Flash**: 32 KB
- **RAM**: 6 KB
- **System Clock**: 12 MHz (HSI internal oscillator)

### Pin Assignments

See pins.h for more.

#### Button
- **BTN** (PA2) - User button with pull-up, triggers EXTI interrupt

#### Badge Power Detection
- **BADGE_PWR_SENSE** (PB6) - Detects when SAO is powered by badge (enables additional LED modes)

#### I2C EEPROM (24LC02B)
- **I2C1_SCL**: PA12 (remapped via SYSCFG)
- **I2C1_SDA**: PA11 (remapped via SYSCFG)

#### UART (USART1 - Pin header)
- **TX**: PA0 (AF4)
- **RX**: PA1 (AF4)
- **Baud Rate**: 115200
- **Config**: 8N1 (8 data bits, no parity, 1 stop bit)

#### UART (USART2 - SAO Connector)
- **TX**: PA4 (AF1) - GPIO2 on SAO connector (pin 6)
- **RX**: PA3 (AF1) - GPIO1 on SAO connector (pin 5)
- **Baud Rate**: 115200 (matches USART1)
- **Config**: 8N1
- **Activation**: Hold the BTN during power-on/reset to enable
- When enabled, console output is mirrored to both UARTs and input is accepted from both

**Note**: To use the SAO UART console, hold down the button while powering on or resetting the badge. The console will be available on both the header UART and the SAO connector simultaneously. This is useful for accessing the console when the badge is connected to a host badge without needing a separate USB-to-serial adapter.

## Build Instructions

### Prerequisites
1. **ARM GCC Toolchain**
   ```bash
   sudo apt-get install gcc-arm-none-eabi
   ```
2. **Flash Tools** (choose one):
   - **st-flash** (from stlink tools):
     ```bash
     sudo apt-get install stlink-tools
     ```
   - **OpenOCD**:
     ```bash
     sudo apt-get install openocd
     ```

### Build Commands

```bash
# Build the project
make

# Clean build artifacts
make clean

# View memory usage
make size

# Generate disassembly listing
make disasm


### Build Output
The build process generates:
- `build/SRAL-SAO2.elf` - ELF executable with debug symbols
- `build/SRAL-SAO2.hex` - Intel HEX format for flashing
- `build/SRAL-SAO2.bin` - Raw binary format
- `build/SRAL-SAO2.map` - Memory map file

## Flashing Instructions

### Using st-flash (ST-Link)
```bash
make flash
```
Or manually:
```bash
st-flash write build/SRAL-SAO2.bin 0x8000000
```

### Using OpenOCD
```bash
make flash-openocd
```
Or manually:
```bash
openocd -f interface/stlink.cfg -f target/stm32c0x.cfg \
    -c "program build/SRAL-SAO2.elf verify reset exit"
```

### Using build.sh Script
The project includes a convenient build and flash script:
```bash
./build.sh          # Build and flash
```

## Flash Read Protection

Our implementation uses **0xBB** for safe Level 1 protection. **NEVER use 0xCC (Level 2) unless you accept permanent loss of debug access - this has been tested and confirmed to be irreversible.**

### Read Protection Levels

- **Level 0 (RDP0)**: No protection - flash can be read freely
- **Level 1 (RDP1)**: Read protection enabled - flash contents cannot be read via debugger or bootloader, but mass erase is still possible to revert to Level 0
- **Level 2 (RDP2)**: **PERMANENT protection** - debug access permanently disabled, JTAG/SWD permanently locked, option bytes can no longer be modified. You cannot read or change the firmware on that MCU anymore, ever.

Devices were provided to users with Level 1 (RDP1) protection set.

### Enabling Read (RDP1) Protection

To enable read protection (RDP Level 1):
```bash
make protect
```

This command will:
1. Prompt for confirmation (operation is destructive if you need to remove it later)
2. Write the RDP option byte to enable Level 1 protection
3. Reset the device with protection enabled

**After enabling protection:**
- Flash contents cannot be read via ST-Link or JTAG/SWD
- **Flashing new firmware:** With RDP active, use STM32CubeProgrammer or ensure your ST-Link has NRST connected for `make flash` to work
- Mass erase is still possible to disable protection

**Testing Read Protection:**

To verify that read protection is actually working:

```bash
make read
```
### Disabling Read Protection

**WARNING**: Disabling read protection requires a mass erase of all flash contents!

```bash
make unprotect
# or
make unprotect-openocd
```

These commands will:
1. Prompt for confirmation
2. Attempt to write RDP Level 0 to the option bytes
3. Trigger a mass erase if successful

**After successful unprotect:**
```bash
make flash  # Reflash the firmware
```

**Note:** The unprotect operation automatically performs a mass erase - the flash will be completely blank after RDP removal. You must reflash your firmware immediately after unprotecting.

## Configuration

### Feature Flags (config.h)
```c
#define SYSTEM_CLOCK_HZ 12000000U   // 12 MHz HSI
#define UART_BAUDRATE 115200U   // UART baud rate
```

### Debugging with GDB

### Start GDB Debug Session
```bash
make debug
```

This will:
1. Start OpenOCD server
2. Connect GDB to the target
3. Load the program
4. Halt at the reset handler

### Manual Debug Setup
Terminal 1 (OpenOCD server):
```bash
openocd -f interface/stlink.cfg -f target/stm32c0x.cfg
```

Terminal 2 (GDB client):
```bash
arm-none-eabi-gdb build/stm32c011_test.elf
(gdb) target extended-remote localhost:3333
(gdb) monitor reset halt
(gdb) load
(gdb) continue
```

## SAO Connector Pinout

The SRAL-SAO2 uses the standard SAO v1.69bis connector:
1. GND
2. VCC (3.3V)
3. SDA (I2C - connected to badge, not currently used by badge)
4. SCL (I2C - connected to badge, not currently used by badge)
5. GPIO1 (USART2 TX - PA4)
6. GPIO2 (USART2 RX - PA3)

## References

https://stm32world.com/wiki/STM32_Readout_Protection_(RDP)

