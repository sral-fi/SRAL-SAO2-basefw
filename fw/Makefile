##############################################################################
# Makefile for SRAL-SAO2 (STM32C011F6P6 based electric badge)
##############################################################################

# Project name
PROJECT = SRAL-SAO2

# Target MCU
MCU = cortex-m0plus
MCU_ARCH = -mcpu=$(MCU) -mthumb

# Toolchain
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

# Directories
BUILD_DIR = build
SRC_DIR = src
INC_DIR = include
CMSIS_CORE_DIR = STM32CubeC0/Drivers/CMSIS/Core/Include
CMSIS_DEVICE_DIR = STM32CubeC0/Drivers/CMSIS/Device/ST/STM32C0xx/Include

# Source files
C_SOURCES = \
src/main.c \
src/i2c_eeprom.c \
src/syscalls.c \
src/cli.c \
src/uart.c \
src/led.c \
src/timer.c \
src/gpio.c \
src/system.c \
STM32CubeC0/Drivers/CMSIS/Device/ST/STM32C0xx/Source/Templates/system_stm32c0xx.c

ASM_SOURCES = \
STM32CubeC0/Drivers/CMSIS/Device/ST/STM32C0xx/Source/Templates/gcc/startup_stm32c011xx.s

# Include paths
INCLUDES = \
-I. \
-I$(INC_DIR) \
-I$(CMSIS_CORE_DIR) \
-I$(CMSIS_DEVICE_DIR)

# Defines
BUILD_DATE = $(shell date +'%Y-%m-%d %H:%M %Z')
DEFINES = \
-DSTM32C011xx \
-DBOARD_SRAL_SAO2 \
-DUSE_FULL_ASSERT \
-DBUILD_DATE="\"$(BUILD_DATE)\""

# Compiler flags
CFLAGS = $(MCU_ARCH) $(DEFINES) $(INCLUDES)
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -g -gdwarf-2
CFLAGS += -Os

# Assembler flags
ASFLAGS = $(MCU_ARCH) $(INCLUDES)
ASFLAGS += -g -gdwarf-2

# Linker flags
LDFLAGS = $(MCU_ARCH)
LDFLAGS += -specs=nano.specs
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map,--cref
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,--print-memory-usage

# Linker script
LDSCRIPT = STM32C011F6P6.ld

# Libraries
LIBS = -lc -lm -lnosys

##############################################################################
# Build targets
##############################################################################

# Default target
all: $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).hex $(BUILD_DIR)/$(PROJECT).bin
	@echo "=== Build complete ==="
	@$(SZ) $(BUILD_DIR)/$(PROJECT).elf

# Object files
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

# Dependency files
DEPS = $(OBJECTS:.o=.d)

# Create build directory
$(BUILD_DIR):
	mkdir -p $@

# Compile C sources
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "CC $<"
	@$(CC) -c $(CFLAGS) -MMD -MP -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

# Assemble ASM sources
$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@echo "AS $<"
	@$(AS) -c $(ASFLAGS) $< -o $@

# Link
$(BUILD_DIR)/$(PROJECT).elf: $(OBJECTS) Makefile
	@echo "LD $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@

# Generate hex file
$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@echo "HEX $@"
	@$(HEX) $< $@

# Generate binary file
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@echo "BIN $@"
	@$(BIN) $< $@

# Clean
clean:
	-rm -rf $(BUILD_DIR)

# Flash the target (requires st-flash from stlink tools)
# Automatically uses --connect-under-reset for RDP-protected devices
flash: all
	@echo "Attempting to flash (will auto-detect if device is protected)..."
	@st-flash write $(BUILD_DIR)/$(PROJECT).bin 0x8000000 || \
		{ echo "Normal flash failed, trying with --connect-under-reset for protected device..."; \
		  st-flash --connect-under-reset --freq=950k write $(BUILD_DIR)/$(PROJECT).bin 0x8000000; }

# Flash prebuilt D26 image from repo (no build)
flash-d26:
	@echo "Flashing prebuilt D26 image: bin-builds/SRAL-SAO2-v147-D26.bin"
	@st-flash write bin-builds/SRAL-SAO2-v147-D26.bin 0x8000000 || \
		{ echo "Normal flash failed, trying with --connect-under-reset for protected device..."; \
		  st-flash --connect-under-reset --freq=950k write bin-builds/SRAL-SAO2-v147-D26.bin 0x8000000; }

# Flash using OpenOCD (alternative)
flash-openocd: all
	openocd -f interface/stlink.cfg -f target/stm32c0x.cfg \
		-c "program $(BUILD_DIR)/$(PROJECT).elf verify reset exit"

# Mass erase flash (requires st-flash from stlink tools)
erase-flash:
	@echo "=========================================="
	@echo "  Flash Mass Erase"
	@echo "=========================================="
	@echo ""
	@echo "WARNING: This will erase ALL flash contents!"
	@echo ""
	@printf "Continue? [y/N] "; \
	read REPLY; \
	case "$$REPLY" in \
		[Yy]*) \
			echo ""; \
			echo "Performing mass erase..."; \
			st-flash --connect-under-reset --freq=950k erase || \
				{ echo "Mass erase failed."; exit 1; }; \
			echo "Mass erase completed successfully."; \
			;; \
		*) \
			echo "Operation cancelled."; \
			;; \
	esac

# Enable flash read protection (RDP Level 1)
# This prevents reading flash contents but allows mass erase
# CRITICAL: RDP values per STM32 specification:
#           0xAA = Level 0 (no protection), complement 0x55, full value: 0xffff55aa
#           0xBB = Level 1 (reversible), complement 0x44, full value: 0xffff44bb
#           0xCC = Level 2 (PERMANENT - VERIFIED ON HARDWARE TO BE IRREVERSIBLE!)
#                  complement 0x33, full value: 0xffff33cc
#                  NEVER USE Level 2 unless you accept permanent loss of debug access!
# Option byte format: 0xRRRRCCVV where CC is complement of VV (RDP byte)
# We use 0xBB for safe, reversible Level 1 protection
protect:
	@echo "Enabling flash read protection (RDP Level 1)..."
	@echo "WARNING: Flash contents will NOT be readable after this operation!"
	@echo "Mass erase will still be possible to remove protection."
	@printf "Continue? [y/N] "; \
	read REPLY; \
	case "$$REPLY" in \
		[Yy]*) \
			st-flash --connect-under-reset --freq=950k --area=option write 0xffff44bb; \
			echo "Read protection enabled. Device will reset."; \
			;; \
		*) \
			echo "Operation cancelled."; \
			;; \
	esac

# Disable flash read protection (requires mass erase)
# WARNING: This will erase all flash contents!
unprotect:
	@echo "=========================================="
	@echo "  Flash Read Protection Removal"
	@echo "=========================================="
	@echo ""
	@echo "WARNING: This will MASS ERASE all flash!"
	@echo ""
	@printf "Continue? [y/N] "; \
	read REPLY; \
	case "$$REPLY" in \
		[Yy]*) \
			echo ""; \
			echo "Attempting to disable read protection..."; \
			st-flash --connect-under-reset --freq=950k --area=option write 0xffff55aa && \
				{ echo ""; \
				  echo "SUCCESS! Read protection removed."; \
				  echo "Device has been mass erased. Reflash firmware with: make flash"; } || \
				{ echo ""; \
				  echo "FAILED: Could not remove protection."; \
				  echo "Try alternative method: make unprotect-openocd"; \
				  echo "Or use STM32CubeProgrammer GUI (see README.md)"; }; \
			;; \
		*) \
			echo "Operation cancelled."; \
			;; \
	esac

# Alternative: OpenOCD method
# Note: stm32c0x is the correct target for STM32C0 in OpenOCD
# (STM32C0/G0/L4 share the same flash controller architecture)
# reference: https://stackoverflow.com/questions/32509747/stm32-read-out-protection-via-openocd
unprotect-openocd:
	@echo "=========================================="
	@echo "  OpenOCD Flash Protection Removal"
	@echo "=========================================="
	@echo ""
	@echo "WARNING: This will MASS ERASE all flash!"
	@echo ""
	@printf "Continue? [y/N] "; \
	read REPLY; \
	case "$$REPLY" in \
		[Yy]*) \
			echo ""; \
			echo "Attempting to disable read protection with OpenOCD..."; \
			openocd -f interface/stlink.cfg -c "adapter speed 950" -f stm32c0x.cfg \
			    -c "init" -c "reset halt" -c "stm32c0x unlock 0" -c "exit" && \
				{ echo ""; \
				  echo "SUCCESS! Read protection removed."; \
				  echo "Device has been mass erased. Reflash firmware with: make flash"; } || \
				{ echo ""; \
				  echo "FAILED: Could not remove protection with OpenOCD."; \
				  echo "Try STM32CubeProgrammer GUI (see README.md)"; }; \
			;; \
		*) \
			echo "Operation cancelled."; \
			;; \
	esac

# Test reading flash memory (to verify read protection works)
read:
	@echo "=========================================="
	@echo "  Flash Memory Read Test"
	@echo "=========================================="
	@echo ""
	@echo "Attempting to read flash memory from address 0x08000000..."
	@echo "If read protection is enabled, this should FAIL."
	@echo ""
	@st-flash --connect-under-reset --freq=950k read /tmp/flash_dump.bin 0x08000000 0x8000 && \
		{ echo ""; \
		  echo "SUCCESS: Flash memory was readable!"; \
		  echo "Read Protection is NOT active (or read test bypassed it)."; \
		  echo "Flash dump saved to: /tmp/flash_dump.bin"; \
		  ls -lh /tmp/flash_dump.bin; \
		  echo ""; \
		  echo "First 256 bytes (hex):"; \
		  hexdump -C /tmp/flash_dump.bin | head -16; } || \
		{ echo ""; \
		  echo "FAILED: Cannot read flash memory."; \
		  echo "Read Protection is ACTIVE and working correctly!"; \
		  echo "This is expected behavior when RDP Level 1 is enabled."; }

# Check read protection status
check-rdp:
	@echo "Checking read protection status..."
	@echo "NOTE: If device is protected, this command will fail (expected behavior)"
	@st-flash --connect-under-reset --freq=950k --area=option read 2>/dev/null | tail -1 | \
		{ read optbytes; \
		  case "$$optbytes" in \
		    *aa) echo "Read Protection: DISABLED (RDP Level 0 - 0xAA)";; \
		    *bb) echo "Read Protection: ENABLED (RDP Level 1 - 0xBB)";; \
		    *cc) echo "Read Protection: PERMANENT (RDP Level 2 - 0xCC - IRREVERSIBLE!)";; \
		    *) echo "Read Protection: STATUS UNKNOWN or DEVICE PROTECTED (cannot read)";; \
		  esac; } || \
		echo "Read Protection: LIKELY ENABLED (device refuses connection)"

# Debug with OpenOCD and GDB
debug:
	@echo "Starting OpenOCD server..."
	@openocd -f interface/stlink.cfg -f target/stm32c0x.cfg &
	@sleep 2
	@echo "Starting GDB..."
	@$(PREFIX)gdb $(BUILD_DIR)/$(PROJECT).elf \
		-ex "target extended-remote localhost:3333" \
		-ex "monitor reset halt" \
		-ex "load" \
		-ex "monitor reset halt"

# Print size information
size: $(BUILD_DIR)/$(PROJECT).elf
	@$(SZ) $<

# Disassemble
disasm: $(BUILD_DIR)/$(PROJECT).elf
	@$(PREFIX)objdump -d $< > $(BUILD_DIR)/$(PROJECT).disasm
	@echo "Disassembly written to $(BUILD_DIR)/$(PROJECT).disasm"

# Terminal connection
term:
	@echo "Starting minicom terminal..."
	@minicom -D /dev/ttyUSB0 -b 115200

# Batch flash: flash and protect in sequence, loop until Ctrl+C
batch-flash: all
	@echo "=========================================="
	@echo "  Batch Flash Operation"
	@echo "=========================================="
	@echo ""
	@echo "This will flash and protect devices in a loop."
	@echo "For each device:"
	@echo "  1. Flash the firmware"
	@echo "  2. Enable flash read protection (RDP Level 1)"
	@echo ""
	@echo "Press Enter to start, or Ctrl+C to abort..."
	@read dummy
	@echo ""
	@while true; do \
		echo "========================================"; \
		echo "Ready to flash next device"; \
		echo "========================================"; \
		echo ""; \
		echo "Connect device and press Enter (or Ctrl+C to stop)..."; \
		read dummy || break; \
		echo ""; \
		echo "Step 1/2: Flashing firmware..."; \
		st-flash --connect-under-reset --freq=950k write $(BUILD_DIR)/$(PROJECT).bin 0x8000000 || \
			{ echo "Failed to flash firmware. Skipping protection."; continue; }; \
		echo "Firmware flashed successfully."; \
		echo ""; \
		echo "Step 2/2: Enabling flash read protection (RDP Level 1)..."; \
		st-flash --connect-under-reset --freq=950k --area=option write 0xffff44bb || \
			{ echo "Failed to enable protection."; continue; }; \
		echo "Read protection enabled. Device will reset."; \
		echo ""; \
		echo "Device completed successfully!"; \
		echo ""; \
		sleep 1; \
	done; \
	echo ""; \
	echo "Batch flash operation ended."

# Help
help:
	@echo "SRAL-SAO2 firmware Makefile Targets:"
	@echo "  all                     - Build the project (default)"
	@echo "  clean                   - Remove build artifacts"
	@echo "  flash                   - Flash using st-flash"
	@echo "  flash-d26               - Flash prebuilt D26 image (bin-builds/SRAL-SAO2-v146-D26.bin)"
	@echo "  flash-openocd           - Flash using OpenOCD"
	@echo "  erase-flash             - Mass erase flash using st-flash"
	@echo "  protect                 - Enable flash read protection (RDP Level 1)"
	@echo "  unprotect               - Disable RDP with st-flash"
	@echo "  unprotect-openocd       - Disable RDP with OpenOCD"
	@echo "  read                    - Test reading flash (verify protection works)"
	@echo "  check-rdp               - Check current read protection status"
	@echo "  debug                   - Start OpenOCD and GDB for debugging"
	@echo "  size                    - Show memory usage"
	@echo "  disasm                  - Generate disassembly listing"
	@echo "  term                    - Start minicom terminal (/dev/ttyUSB0, 115200 8N1)"
	@echo "  batch-flash             - Protect and flash in one operation (with confirmation)"
	@echo ""
	@echo "Required tools:"
	@echo "  - arm-none-eabi-gcc toolchain"
	@echo "  - st-flash (from stlink tools) or OpenOCD"
	@echo ""

.PHONY: all clean flash flash-d26 flash-openocd erase-flash debug size disasm help protect unprotect unprotect-openocd read check-rdp term batch-flash

# Dependencies
-include $(wildcard $(BUILD_DIR)/*.d)
