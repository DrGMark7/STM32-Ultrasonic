# STM32 Ultrasonic Distance Sender

Firmware for an `STM32L432KC` (`NUCLEO-L432KC`) that reads distance from an HC-SR04-style ultrasonic sensor, smooths the result with a moving average filter, prints the measurement over UART, and sends the distance to an ESP32 over I2C.

## Overview

The application does the following in a loop:

- Triggers the ultrasonic sensor with a 10 us pulse.
- Measures the ECHO pulse width using `TIM2` as a 1 MHz free-running timer.
- Converts pulse width to centimeters with `distance_cm = echo_us / 58`.
- Applies a 5-sample moving average filter.
- Sends the filtered distance to an ESP32 over I2C.
- Prints status and distance messages over `USART2` at `115200` baud.

If the measurement times out or falls outside the configured range, the firmware reports `OUT OF RANGE` over UART and transmits `9999` over I2C.

## Hardware

Target MCU and board:

- MCU: `STM32L432KCUx`
- Board: `NUCLEO-L432KC`
- Clock: `32 MHz` system clock

Main interfaces used by the project:

- Ultrasonic sensor trigger: `PA0`
- Ultrasonic sensor echo: `PA1`
- UART debug TX: `PA2`
- UART debug RX: `PA15`
- I2C1 SCL: `PB6`
- I2C1 SDA: `PB7`

Expected peripherals:

- HC-SR04 or compatible ultrasonic sensor
- ESP32 configured as an I2C device at address `0x08`

## Firmware Behavior

Important constants from the current implementation:

- I2C target address: `0x08` (`0x10` in STM32 HAL 8-bit shifted form)
- UART baud rate: `115200`
- Filter size: `5`
- Echo timeout: `38000 us`
- Max distance clamp: `400 cm`
- Measurement interval: `100 ms`

Startup messages:

- `System Ready`
- `ESP32 Found!` or `ESP32 NOT Found`

I2C payload format:

- 4 bytes
- Little-endian unsigned integer
- Value is the filtered distance in centimeters
- `9999` indicates out-of-range or invalid measurement

## Project Structure

- [`test01.ioc`](/Users/drgmark7/Desktop/Code/STM32-Ultrsonic/test01.ioc): STM32CubeMX configuration
- [`Core/Src/main.c`](/Users/drgmark7/Desktop/Code/STM32-Ultrsonic/Core/Src/main.c): application logic
- [`Core/Inc/main.h`](/Users/drgmark7/Desktop/Code/STM32-Ultrsonic/Core/Inc/main.h): main header
- [`CMakeLists.txt`](/Users/drgmark7/Desktop/Code/STM32-Ultrsonic/CMakeLists.txt): top-level build configuration
- [`CMakePresets.json`](/Users/drgmark7/Desktop/Code/STM32-Ultrsonic/CMakePresets.json): preset-based configure/build flow
- [`cmake/gcc-arm-none-eabi.cmake`](/Users/drgmark7/Desktop/Code/STM32-Ultrsonic/cmake/gcc-arm-none-eabi.cmake): ARM GCC toolchain definition
- [`STM32L432XX_FLASH.ld`](/Users/drgmark7/Desktop/Code/STM32-Ultrsonic/STM32L432XX_FLASH.ld): linker script

## Build

Requirements:

- `CMake >= 3.22`
- `Ninja`
- `arm-none-eabi-gcc` toolchain available in `PATH`

Configure and build a debug image:

```sh
cmake --preset Debug
cmake --build --preset Debug
```

Configure and build a release image:

```sh
cmake --preset Release
cmake --build --preset Release
```

Build output:

- ELF: `build/Debug/test01.elf` or `build/Release/test01.elf`
- Map file: `build/<Preset>/test01.map`

## Flashing

This repository does not currently include a flashing script. Typical options are:

- STM32CubeIDE / STM32CubeProgrammer
- `st-flash`
- `openocd`

Example with `st-flash`:

```sh
st-flash write build/Debug/test01.elf 0x8000000
```

Use the tool and image format that matches your local flashing setup.

## Regenerating Code

The project was generated with STM32CubeMX using CMake as the target toolchain. If you regenerate code:

- Open [`test01.ioc`](/Users/drgmark7/Desktop/Code/STM32-Ultrsonic/test01.ioc) in STM32CubeMX or STM32CubeIDE.
- Keep `ProjectManager.KeepUserCode=true` enabled.
- Re-generate the project files.
- Rebuild with CMake.

User application logic is primarily in the `USER CODE` sections of [`Core/Src/main.c`](/Users/drgmark7/Desktop/Code/STM32-Ultrsonic/Core/Src/main.c).

## Notes and Limitations

- The ultrasonic measurement uses polling, not input capture interrupts.
- The filter buffer starts at zero, so the first few readings are biased low until the buffer fills.
- I2C transmission is performed as STM32 master transmit. The ESP32 side must be configured accordingly.
- The code sends raw integer centimeters only; there is no checksum or framing beyond the 4-byte payload.
