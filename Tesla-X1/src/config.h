#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// -- Ultrasonic Sensor Pins ---
#define PIN_SDA 21 // D4
#define PIN_SCL 22 // D5
#define STM32_I2C_ADDR 0x08

#define STOP_DISTANCE 5.0  // Stop distance for break (cm)
#define LOG_DISTANCE 30.0   // Distance for start Log (cm)

// --- Motor Pins ---
#define M1_PWM 4
#define M1_IN1 17
#define M1_IN2 16

#define M2_PWM 19
#define M2_IN1 5
#define M2_IN2 18

// --- PWM Settings ---
#define PWM_FREQ 5000
#define PWM_RES 8  // Bit range for PWM 8 bit (0-255)

// -- Buuzzer Pin ---
#define BUZZER_PIN 23

// --- Driving Physics (Tune here!) ---
#define ACCEL_STEP 0.9      // Accelarate in Gear D
#define BRAKE_STEP 1.8      // Deceleration (if much will stop early)

// --- Network Config ---
// Remote MAC Addr
const uint8_t REMOTE_ADDR[] = {0xE8, 0xD4, 0x84, 0x00, 0xFB, 0x3C};
// const uint8_t REMOTE_ADDR[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#endif