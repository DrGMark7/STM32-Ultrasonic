# Tesla Model X Embedded System Project

Integrated embedded project for a small vehicle platform built from three cooperating controllers:

- `ESP32-Remote` handles the handheld controller UI, motion sensing, joystick input, and `ESP-NOW` command transmission.
- `Tesla-X1` runs on the vehicle-side ESP32 and controls motors, buzzer state, and emergency braking logic.
- `STM32-Ultrasonic` measures obstacle distance with an ultrasonic sensor and forwards that value to the vehicle controller over `I2C`.

## System Overview

![Vehicle wiring and controller flow](./assets/CarDiagram.png)

The system is split into a wireless control path and a wired safety path:

1. The remote ESP32 reads joystick, buttons, and MPU6050 tilt data.
2. The remote sends gear, safety, horn, and drive packets to the vehicle over `ESP-NOW`.
3. The STM32 continuously measures obstacle distance with an HC-SR04 style sensor.
4. The STM32 transmits distance data to the vehicle ESP32 over `I2C`.
5. The vehicle ESP32 drives the motors and applies an emergency stop if safety mode is enabled and an obstacle is too close.

## Remote Controller

![Remote controller layout](./assets/RemoteDiagram.png)

Main implementation: [final.py](/Users/drgmark7/Desktop/Code/Tesla-Model-X-All-part/ESP32-Remote/final.py)

Current remote behavior:

- Runs on MicroPython with `espnow`, `network`, `machine`, `ssd1306`, and a local `mpu6050` driver
- Uses MPU6050 roll angle as steering input
- Uses joystick throttle for forward power
- Toggles between `N` and drive mode with a gear button
- Switches between `D` and `S` while in drive mode
- Uses a separate lever input to enter reverse
- Sends horn and safety toggles to the car
- Draws a live UI on a `128x64` OLED

Packet types sent by the remote:

- `0`: gear update
- `1`: safety mode update
- `2`: driving data payload
- `3`: horn / buzzer state

`DrivingData` payload on the car side:

```cpp
struct DrivingData {
    float angle;
    int power;
};
```

## Vehicle Controller

Vehicle firmware lives in [main.cpp](/Users/drgmark7/Desktop/Code/Tesla-Model-X-All-part/Tesla-X1/src/main.cpp) with configuration in [config.h](/Users/drgmark7/Desktop/Code/Tesla-Model-X-All-part/Tesla-X1/src/config.h).

Responsibilities:

- Receive remote commands over `ESP-NOW`
- Drive two DC motors with PWM and direction pins
- Listen for 4-byte distance updates from the STM32 over `I2C` address `0x08`
- Engage an emergency brake when safety mode is on and distance is below `STOP_DISTANCE`
- Send emergency-brake feedback back to the remote peer

Current vehicle pin mapping:

| Function    | GPIO |
| ----------- | ---: |
| I2C SDA     |   21 |
| I2C SCL     |   22 |
| Motor 1 PWM |    4 |
| Motor 1 IN1 |   17 |
| Motor 1 IN2 |   16 |
| Motor 2 PWM |   19 |
| Motor 2 IN1 |    5 |
| Motor 2 IN2 |   18 |
| Buzzer      |   23 |

Drive behavior:

- `N`: motor outputs off
- `D`: forward with smoothed acceleration and braking
- `S`: forward with immediate target PWM response
- `R`: reverse

Safety behavior:

- Distance values are stored in `globalDistance`
- If safety mode is enabled and the gear is `D` or `S`, the car checks obstacle distance
- If distance is greater than `1 cm` and less than `STOP_DISTANCE` (`5 cm`), both motors are electrically braked and PWM is set to zero

## Ultrasonic Sensor Node

STM32 firmware entry point: [main.c](/Users/drgmark7/Desktop/Code/Tesla-Model-X-All-part/STM32-Ultrasonic/Core/Src/main.c)

Current STM32 behavior:

- Generates a `10 us` trigger pulse for the ultrasonic sensor
- Measures echo width using `TIM2`
- Converts pulse width to centimeters
- Applies a 5-sample moving average filter
- Sends the filtered distance to the vehicle ESP32 over `I2C`
- Prints status over `USART2` at `115200`

Key STM32 hardware assumptions:

- Board: `NUCLEO-L432KC`
- MCU: `STM32L432KCUx`
- Trigger pin: `PA0`
- Echo pin: `PA1`
- UART TX: `PA2`
- UART RX: `PA15`
- I2C SCL: `PB6`
- I2C SDA: `PB7`

Distance transport format:

- 4-byte little-endian unsigned integer
- Value is distance in centimeters
- `9999` means invalid or out of range

## Repository Layout

```text
.
|-- assets/
|-- ESP32-Remote/
|   |-- final.py
|   |-- mpu6050.py
|   `-- ssd1306.py
|-- STM32-Ultrasonic/
|   |-- Core/
|   |-- Drivers/
|   |-- CMakeLists.txt
|   `-- README.md
`-- Tesla-X1/
    |-- src/
    |-- platformio.ini
    `-- README.md
```

## Build And Flash

### 1. Vehicle ESP32 (`Tesla-X1`)

Requirements:

- PlatformIO Core or VS Code with the PlatformIO extension
- ESP32 development board connected over USB

Commands:

```bash
cd Tesla-X1
pio run
pio run -t upload
pio device monitor -b 115200
```

### 2. STM32 ultrasonic node (`STM32-Ultrasonic`)

Requirements:

- `CMake >= 3.22`
- `Ninja`
- `arm-none-eabi-gcc`

Commands:

```bash
cd STM32-Ultrasonic
cmake --preset Debug
cmake --build --preset Debug
```

Flash with your usual STM32 tooling such as STM32CubeProgrammer, `st-flash`, or `openocd`.

### 3. Remote ESP32 (`ESP32-Remote`)

Requirements:

- ESP32 board flashed with MicroPython
- MicroPython file upload workflow such as `mpremote`, Thonny, or ampy-compatible tooling

Files to deploy:

- `final.py`
- `mpu6050.py`
- `ssd1306.py`

Before running, confirm the `peer` MAC address in [final.py](/Users/drgmark7/Desktop/Code/Tesla-Model-X-All-part/ESP32-Remote/final.py) matches the vehicle ESP32 MAC address, and confirm `REMOTE_ADDR` in [config.h](/Users/drgmark7/Desktop/Code/Tesla-Model-X-All-part/Tesla-X1/src/config.h) matches the remote.

## Integration Notes

- `ESP-NOW` is configured for Wi-Fi channel `1` on both ESP32 sides
- The vehicle expects `I2C` address `0x08`
- The remote and vehicle MAC addresses are currently hardcoded
- The root repository includes generated build artifacts under `Tesla-X1/build/`; these are not source files

## Known Gaps

- The remote setup does not yet have its own standalone README
- Reverse behavior while safety mode is active is marked in source as needing debugging
- No automated tests are included for any of the three firmware targets
- `ESP-NOW` traffic is currently unencrypted

## Member

- Napassakorn S. @DrGMark7
- Phongsapak C. @reawphongsaphak
- Sirapat P. @HUTZAKI
