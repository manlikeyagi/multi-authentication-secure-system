# Multi-Authentication Secure System

This project is a **Multi-Authentication Secure System** built using Arduino-based hardware. It integrates **RFID**, **fingerprint scanning**, and **password authentication** to provide a high level of physical access control and security.

## 🔐 Features

- RFID-based user identification
- Biometric fingerprint verification
- Keypad for PIN/password entry
- Secure access control logic
- Real-time authentication feedback via buzzer and LED
- LCD or OLED display for user prompts and feedback

## 🛠️ Components Used

- Arduino Uno / ESP32 / NodeMCU (based on your setup)
- RFID module (RC522)
- Fingerprint sensor (e.g., R305 or GT-521F52)
- 4x4 Keypad
- 16x2 LCD or OLED Display
- Buzzer and LEDs
- Breadboard and jumper wires
- Power source

## 📦 How It Works

1. User presents RFID card/tag.
2. If the card is valid, the system prompts for fingerprint.
3. Upon successful fingerprint scan, the system requests the PIN.
4. Only after all three authentication methods are successful, access is granted.

## 💻 Code

The project is developed in Arduino IDE and written in C/C++ for microcontrollers.






