#!/bin/bash
# Build script for STM32C011F6P6 Peripheral Test Project

set -e  # Exit on error

echo "======================================"
echo "STM32C011F6P6 Build Script"
echo "======================================"

# Check for required tools
echo "Checking required tools..."
if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "ERROR: arm-none-eabi-gcc not found!"
    echo "Install with: sudo apt-get install gcc-arm-none-eabi"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo "ERROR: make not found!"
    echo "Install with: sudo apt-get install make"
    exit 1
fi

echo "✓ All required tools found"
echo ""

# Clean previous build
echo "Cleaning previous build..."
make clean
echo ""

# Build the project
echo "Building project..."
make
echo ""

# Show build results
echo "======================================"
echo "Build Summary"
echo "======================================"
arm-none-eabi-size build/stm32c011_test.elf

echo ""
echo "Build artifacts:"
ls -lh build/*.bin build/*.hex build/*.elf 2>/dev/null || true
echo ""

echo "======================================"
echo "Build completed successfully!"
echo "======================================"
echo ""
echo "Flash with:"
echo "  st-flash write build/stm32c011_test.bin 0x8000000"
echo "  OR"
echo "  make flash"
