# Project Context

## Target Hardware
- **Primary MCU/Processor**: STM32G491xx (Cortex-M4 with FPv4-SP-D16, hard-float ABI)
- **Secondary Processors**:
- **Dev Board / Custom PCB**: NUCLEO-G491RE (STM32CubeMX-generated Makefile project)
- **Board Revision**:

## Clock Configuration
- **HSE/HSI**: HSI only (no PLL)
- **PLL Config**: PLL disabled
- **System Clock Frequency**: 16 MHz (HSI default, FLASH_LATENCY_0)

## Pin Mapping / GPIO Assignments
- PA0 — ADC1 CH1 (analog input, A0 Arduino header)
- PA2 — LPUART1 TX (Virtual COM port)
- PA3 — LPUART1 RX (Virtual COM port)
- PA5 — LED toggle output
- PA10 — Digital input with pull-down (D2 Arduino header, sound sensor)

## Memory Layout
- **Flash Size**:
- **RAM Size**:
- **Linker Script Regions**: STM32G491XX_FLASH.ld

## Peripherals & Interfaces
- ADC1 CH1 (PA0) — 12-bit, software trigger, single conversion
- LPUART1 (PA2/PA3, AF12) — UART to Virtual COM port
- GPIO PA5 — LED blink/toggle
- GPIO PA10 — digital input (sound sensor)

## External Devices & Sensors
- Sound/clap sensor on PA10 (digital) and PA0 (analog)

## Communication Protocols
- **Protocol (e.g. CAN, UART, SPI, I2C)**: LPUART1 (Virtual COM port via ST-Link)
- **Baud Rate / Mode**: 115200 8N1

## Software Stack
- **Build System**: Make, Bazel
- **Compile Command**:
- **HAL / Framework**:
- **RTOS**:
- **Toolchain**:
- **Compiler Flags**:
- **External Libraries**:

## Development Environment
- **Host OS**: Windows
- **IDE**:
- **Debug Interface (SWD/JTAG)**:
- **Debug Probe (ST-Link/J-Link/OpenOCD)**:

## Debug Configuration
<!-- Auto-learned during debug sessions. Edit these directly to change debug settings. -->
- **GDB Server Command**:
- **GDB Executable**:
- **Firmware ELF Path**:
- **GDB Server Port**:

## Power Management
- **Sleep Modes**:
- **Power Domains**:
- **Battery / Power Source**:

## Boot Configuration
- **Bootloader**:
- **OTA Mechanism**:

## Interrupt Configuration
- **NVIC Priority Groups**:
- **Key ISRs**:

## Compliance & Standards
-

## SDK / Library Paths
<!-- Paths to vendor SDKs or libraries the agent cannot download. -->
<!-- Example: /opt/zephyr-sdk-0.16.8, /home/user/STM32CubeF4, ~/ncs/v2.7.0 -->
-

## Supporting Devices
-

## Project Notes

