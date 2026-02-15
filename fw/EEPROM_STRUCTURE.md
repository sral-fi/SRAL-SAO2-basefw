
# SRAL-SAO2 EEPROM Structure

## Overview

The SRAL-SAO2 firmware uses a 24C02 EEPROM (256 bytes total) for persistent storage of configuration data. The EEPROM is accessed via I2C at address 0x50 (7-bit).

## SAO Specification 

Badge.Team "Binary SAO descriptor" spec (https://badge.team/docs/standards/sao/binary_descriptor/) 

- 4 bytes: magic (ASCII "LIFE")
- 1 byte: SAO name length (N)
- 1 byte: first driver name length (DNL)
- 1 byte: first driver data length (DDL)  <-- NOTE: spec uses a single byte here
- 1 byte: number of extra drivers (EDC)
- N bytes: SAO name
- DNL bytes: first driver name
- DDL bytes: first driver data
- (if EDC>0) additional drivers follow

## Implementation in SRAL-SAO2:

- **0x00-0x03**: Magic = "LIFE"
- **0x04**: Name length = 9
- **0x05**: Driver name length = 5
- **0x06**: Driver data length = 0x20 (32)
- **0x07**: Number of extra drivers = 0x00
- **0x08-0x10**: Name = "SRAL-SAO2" (9 bytes)
- **0x11-0x15**: Driver name = "sral2" (5 bytes)
- **0x16-0x35**: Driver data (32 bytes; hex 42 43 44 45 46 30 31 32 33 34 35 36 37 38 39 41 42 43 44 45 46 30)

## Data after SAO part

Per the SRAL-SAO2 layout the SAO descriptor occupies offsets 0x00..0x35 (54 bytes). The remaining EEPROM (256 bytes total) is used by the firmware and begins immediately after the SAO part.

-- SAO end / firmware area start: 0x36 (decimal 54)
-- `[[MARKER]]` magic (ASCII) start: 0x36 (decimal 54)
-- `[[MARKER]]` length: 10 bytes (ASCII "[[MARKER]]") — occupies offsets 0x36..0x3F (decimal 54..63)
-- Firmware internal data start (after [[MARKER]]): 0x40 (decimal 64)
- Firmware internal data length: 256 - 0x40 = 192 bytes (0xC0)

The firmware uses the `[[MARKER]]` area for entropy/keys and the remaining 192 bytes for configuration, state, and other persistent data. Within that firmware region we reserve the first 14 bytes for a null-terminated callsign (max 13 visible characters + NUL), matching the firmware's CLI storage.

- Firmware internal data (0x40..0xFF): 192 bytes total
	- **0x40..0x4D (14 bytes)**: Callsign (null-terminated, up to 13 chars)
	- **0x4E..0x62 (21 bytes)**: CW message slot (null-terminated, up to 20 chars)
	- **0x63..0xEF (141 bytes)**: Remaining firmware persistent data (env, keys, state)

Readers and firmware should treat the callsign area as fixed-length, NUL-terminated ASCII (letters/numbers, '-' and '/'). The remainder of the firmware area may be packed as needed by the firmware and should be treated as opaque by external tools unless documented further.


## Implementation Notes

- EEPROM writes require 5ms delay between operations
- Internal MCU I2C pull-ups are left disabled; board uses external pull-ups
- Configuration is loaded on boot and saved on changes

## Implementation Notes

- EEPROM writes require 5ms delay between operations
- Internal MCU I2C pull-ups are left disabled; board uses external pull-ups
- Configuration is loaded on boot and saved on changes

