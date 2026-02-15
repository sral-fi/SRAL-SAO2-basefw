# SRAL-SAO2 badge evaluation board Firmware

This is the base firmware for the **SRAL-SAO2** (Simple/Sh*tty Add-On version 2), an interactive SAO hybrid badge for ham radio enthusiasts. The firmware runs on the STM32C011F6P6 microcontroller and includes interactive games, LED effects, and I2C EEPROM storage for persistent variables.

## Hardware Configuration

### Target MCU
- **Device**: STM32C011F6P6
- **Core**: ARM Cortex-M0+
- **Flash**: 32 KB
- **RAM**: 6 KB
- **System Clock**: 12 MHz (HSI internal oscillator)

### Pin Assignments

#### LEDs (5x White LEDs)
- **LED1** (PA8) - Top LED
- **LED2** (PA5) - LED 2
- **LED3** (PB7) - LED 3
- **LED4** (PC14) - LED 4
- **LED5** (PC15) - LED 5
- **LED** (PA6) - Debug LED (optional)

#### Button
- **BTN** (PA2) - User button with pull-up, triggers EXTI interrupt

#### Badge Power Detection
- **BADGE_PWR_SENSE** (PB6) - Detects when SAO is powered by badge (enables additional LED modes)

#### I2C EEPROM (24LC02B)
- **I2C1_SCL**: PA12 (remapped via SYSCFG)
- **I2C1_SDA**: PA11 (remapped via SYSCFG)
- Used for non-volatile storage of game progress and high scores

#### UART (USART1 - Header)
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

## Features

### LED Animation Modes
The firmware includes 8 different LED animation modes that can be cycled through by pressing the button:

0. **OFF** - All LEDs off
1. **BLINK** - Random LED blinks with random timing (default mode, starts from boot)
2. **BLINK_FADE** - Random LED fades out smoothly
3. **CONTINUOUS** - All LEDs pulse together
4. **BLINK_ALL** - All LEDs blink in sync 
5. **STROBO** - Rapid strobe effect 
6. **INTENCIRCLE** - Intense circle pattern
7. **DISCO** - Another intense led flashing mode

### Interactive CLI
The firmware includes a full-featured command-line interface accessible via UART:

#### Application commands

- (no applications in the base firmware)

#### Generic commands

- `help` or `?` - Display command help
- `uptime` - Show system uptime
- `info` - Display system information
- `version` - Show firmware version

#### LED Control Commands
- `led <n> on|off|toggle` - Control individual LEDs (1-5)
- `mode <n>` - Set LED animation mode (0-7)
- `test` - Run LED test sequence

## Build Instructions

### Prerequisites
1. **ARM GCC Toolchain**
   ```bash
   sudo apt-get install gcc-arm-none-eabi
   ```

2. **Build Tools**
   ```bash
   sudo apt-get install make python3
   ```

3. **Flash Tools** (choose one):
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

# Validate token dictionary (used by HamQuest)
make validate-tokens
```

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
- **Level 2 (RDP2)**: **PERMANENT protection - TESTED AND VERIFIED IRREVERSIBLE** - debug access permanently disabled, JTAG/SWD permanently locked, option bytes can no longer be modified. **DO NOT USE except on final production devices where you absolutely need maximum security and accept that the device can never be reprogrammed or debugged again.**

### Enabling Read Protection

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
- Mass erase is still possible to disable protection (requires STM32CubeProgrammer)

**Testing Read Protection:**

To verify that read protection is actually working:

```bash
make read
```

This command attempts to read flash memory. If protection is enabled, it will fail (which is expected and correct behavior). If protection is disabled, it will successfully read and display the flash contents.

**Important Note on Hardware:**
- If your ST-Link adapter does **not** have the NRST (reset) pin connected, you will need STM32CubeProgrammer to flash protected devices
- The `make flash` command with `--connect-under-reset` requires a physical reset connection to work reliably

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

**Note**: If protection is already active, these commands will likely fail because the device blocks debugger access. Use STM32CubeProgrammer GUI (Method 1) if these fail.

**After successful unprotect:**
```bash
make flash  # Reflash the firmware
```

**Note:** The unprotect operation automatically performs a mass erase - the flash will be completely blank after RDP removal. You must reflash your firmware immediately after unprotecting.

### Checking Protection Status

To check the current read protection level:
```bash
make check-rdp
```

### When to Use Read Protection

**Use read protection when:**
- Deploying production firmware
- Protecting proprietary game content or algorithms
- Preventing unauthorized duplication of badges
- Securing intellectual property
- **You have STM32CubeProgrammer available for removal**

**Do NOT use read protection when:**
- Still in active development
- You need to debug with GDB
- You want to verify flash contents
- Working with prototypes
- **You don't have STM32CubeProgrammer installed**
- You need to quickly iterate and reflash often

⚠️ **IMPORTANT WARNING**: Once RDP Level 1 is enabled, you will need STM32CubeProgrammer (GUI application) to remove it. Command-line tools (`st-flash`, `openocd`) generally cannot connect to protected devices. Plan accordingly!

## Configuration

### Feature Flags (config.h)
```c
#define SYSTEM_CLOCK_HZ 12000000U   // 12 MHz HSI
#define UART_BAUDRATE 115200U   // UART baud rate
```

## Development

### Adding New Commands
1. Add command handler in `src/cli.c`
2. Update help text in CLI
3. Rebuild and test

### Modifying LED Patterns
LED animation modes are in `src/main.c` in the main loop. Each mode has its own logic block.

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

## Testing the Device

### Serial Terminal Setup
Connect a USB-to-serial adapter to the UART pins:
- **TX** (PA0) → RX on USB-serial adapter (for USART1 header)
- **RX** (PA1) → TX on USB-serial adapter (for USART1 header)
- **GND** → GND

Use a serial terminal program:
```bash
# Using screen
screen /dev/ttyUSB0 115200

# Using minicom
minicom -D /dev/ttyUSB0 -b 115200

### Button Operation
- **Short press**: Cycle through LED animation modes

## Troubleshooting

### Build Errors
- **Missing toolchain**: Install `gcc-arm-none-eabi`
- **CMSIS headers not found**: Ensure `STM32CubeC0/` submodule is initialized
- **Python errors**: Install Python 3 for token validation scripts
- **Memory overflow**: Disable HAMQUEST in `config.h` to save memory

### Flash Errors
- **st-flash not found**: Install `stlink-tools`
- **No ST-Link detected**: Check USB connection and ensure ST-Link drivers are installed
- **Permission denied**: Add user to `dialout` group: `sudo usermod -a -G dialout $USER`
- **Flash write failed**: Try erasing the chip first: `st-flash erase`

### Serial Terminal Issues
- **No output**: Check baud rate (115200), TX/RX connections, and GND
- **Garbled text**: Verify baud rate matches (115200 8N1)
- **No device**: Check permissions and that USB-serial adapter is recognized
- **Wrong UART pins**: This firmware uses USART1 on PA0/PA1, not USART2

### LED Issues
- **Some modes not working**: Modes 3-7 require BADGE_PWR_SENSE to be high
- **Button not responding**: Check EXTI interrupt configuration and debouncing

### EEPROM Issues
- **Game not saving**: Check I2C connections and pull-ups on PA11/PA12
- **I2C errors**: Ensure SYSCFG remapping is enabled for PA11/PA12
- **Corrupted data**: Use CLI command `reset-scores` to clear EEPROM

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

## License

This project uses:
- CMSIS headers (ARM/STMicroelectronics license)
- STM32CubeC0 HAL (STMicroelectronics BSD license)
- application code (TBD)

