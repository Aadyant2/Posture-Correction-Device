# Wearable Posture Sensor - Hardware & Schematic Guide

## Overview
A wearable dual-IMU spinal posture tracking device with real-time ERM haptic biofeedback powered by the ultra-low-power **Seeed Studio XIAO ESP32-C3**.

## Bill of Materials (Exact 8 Parts)
1. **U1: Seeed Studio XIAO ESP32-C3** (Microcontroller with BLE & LiPo charge management)
2. **U2: MPU6050 (GY-521) IMU Lower** (Hardware address **0x68** via AD0 -> GND)
3. **U3: MPU6050 (GY-521) IMU Upper** (Hardware address **0x69** via AD0 -> 3V3)
4. **U4: DRV2605L Haptic Driver Breakout** (Hardware address **0x5A**, EN -> 3V3)
5. **M1: Coin Vibration Motor (ERM)** (Connected to DRV2605L OUT+ and OUT-)
6. **BT1: 3.7V LiPo Battery** (Single cell, 350-500mAh)
7. **SW1: SPST Slide Switch** (Master power isolation between LiPo (+) and XIAO BAT+)
8. **SW2: Momentary Pushbutton** (Posture zero-tare on XIAO D1 / GPIO3 to GND)
9. **R1 (220 Ω) + D1 (LED)** (Current-limited slouch warning on XIAO D2 / GPIO4 to GND)

## Exact Wiring Table
| Component | Pin | Connected Net | Destination Pin |
|---|---|---|---|
| **XIAO ESP32-C3** | 3V3 | `3V3` | IMU Lower VCC, IMU Upper VCC, DRV2605L VIN, DRV2605L EN, IMU Upper AD0 |
| **XIAO ESP32-C3** | GND | `GND` | All module GNDs, IMU Lower AD0, Button Pin 2, LED Cathode, Battery (-) |
| **XIAO ESP32-C3** | D4 (GPIO6) | `SDA` | IMU Lower SDA, IMU Upper SDA, DRV2605L SDA |
| **XIAO ESP32-C3** | D5 (GPIO7) | `SCL` | IMU Lower SCL, IMU Upper SCL, DRV2605L SCL |
| **XIAO ESP32-C3** | D1 (GPIO3) | `BTN_CALIB` | Pushbutton SW2 Pin 1 (Internal pull-up enabled in code) |
| **XIAO ESP32-C3** | D2 (GPIO4) | `LED_SIG` | Resistor R1 (220 Ω) Pin 1 |
| **Resistor R1** | Pin 2 | `LED_ANODE` | LED D1 Anode (Cathode to GND) |
| **DRV2605L** | OUT+ | `HAPTIC_OUT_P` | Coin Motor Terminal 1 (+) |
| **DRV2605L** | OUT- | `HAPTIC_OUT_N` | Coin Motor Terminal 2 (-) |
| **Battery BT1 (+)**| (+) | `BAT_RAW` | Slide Switch SW1 Pin 1 |
| **Slide Switch** | Pin 2 | `BAT_SW` | XIAO BAT+ pad |
| **Battery BT1 (-)**| (-) | `GND` | XIAO BAT- pad & common GND |

## Rules Followed
- **INT Pins**: Unconnected on both IMUs (polled I2C architecture prevents pin starvation).
- **No Double Pin Assignments**: Every MCU pin has a dedicated function.
- **3.3V Logic Level**: All breakouts run at unified 3.3V logic.

## Software
Use the included `wearable_posture_sensor.ino` sketch in the Arduino IDE or ESP-IDF with the Adafruit MPU6050 and Adafruit DRV2605 libraries.
