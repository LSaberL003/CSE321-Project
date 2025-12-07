# CSE321 Project: Real-Time Fall Detection System

A smart wearable prototype designed to detect falls for the elderly or lone workers. The system uses a multi-stage algorithm to distinguish real falls from daily activities (like running or jumping) and triggers an audio-visual alarm when help is needed.

## Key Features

  Real-Time Monitoring: Non-blocking "Super Loop" architecture ensures <10ms sensor response latency.

  Intelligent Algorithm: Combines Impact Detection (High G) with a Sustained Motion Check (Stability) to filter out false alarms.

  Audio-Visual Alarm: Variable frequency siren (2kHz/1kHz) and strobing LED alert.

  User Interaction: OLED status display and long-press button reset mechanism.

## Project Codebase

### 1. sketch-Loop-V1.ino (Baseline)

  Description: Initial prototype.

  Logic: Simple threshold triggering. Alarms immediately when acceleration exceeds 1.8G.

  Limitation: High false alarm rate. Triggers easily when user jumps, claps hands, or runs.

### 2. sketch-Loop-V2.ino (Final / Optimized)

**Description**: The current, polished version used for the final demo.

**Logic**: Implements a Finite State Machine (FSM).

  1. **Monitor**: Waits for high impact (>1.8G).

  2. **Analyze**: If impact occurs, enters a 5-second judging window to monitor subsequent motion.

  3. **Decision**:

    a. If substantial motion is detected (user stands up/walks) -> False Alarm.

    b. If no/little motion is detected (user is still) -> Fall Confirmed -> Alarm.

  **Optimization**: Removed blocking delay() calls, optimized I2C speed to 400kHz, and throttled display refresh rate for system stability.

## Technical Challenges: The FreeRTOS Attempt

  ### **"Why I switched from FreeRTOS back to Loop Architecture."

One of the major challenges encountered during development was the memory limitation of the Arduino Uno when attempting to implement a Real-Time Operating System (RTOS).

### The Problem

  Hardware Constraint: The Arduino Uno (ATmega328P) has only 2KB (2048 bytes) of SRAM.

  Memory Usage:

    SSD1306 OLED Library: Requires a 1KB (1024 bytes) screen buffer.

    FreeRTOS Kernel: Requires memory for the scheduler and context switching.

    Task Stacks: Each task (Sensor, Alarm, Display) requires its own stack (min 128 bytes each).

### The Result: Stack Overflow & Boot Loop

  When combining FreeRTOS with the OLED library, the RAM usage exceeded 2KB immediately. The system entered a "Boot Loop" or froze completely because there was no memory left for the stack.

### Evidence of Failure:
  As seen in the debug logs below, the system constantly resets after initializing the display, or freezes with minimal free RAM.


  ![9c0558d29a25bcb8a5baf1bcc3a0db16](https://github.com/user-attachments/assets/412d5523-b7a0-4f9d-a63b-88a072acb555)

### The Solution

  I adopted a Non-blocking Super Loop architecture. By using millis() for timing and state machines for logic, I achieved real-time performance without the memory overhead of an OS scheduler.

## Hardware Setup

  Microcontroller: Arduino Uno R3

  Sensor: MPU6050 (Accelerometer + Gyroscope)

  Display: SSD1306 OLED (128x64 I2C)

  Output:

    Active Buzzer (Pin 8)

    LED (Pin 9)

  Input: Push Button (Pin 2)

Wiring:
  Check HardWare_Demo_for_Wokwi.JSON

## How to Run

  1. Open Optimized_Fall_Detector.ino (V2) in Arduino IDE.

  2. Install required libraries:

      Adafruit SSD1306

      Adafruit GFX

      Wire

  3. Upload to Arduino Uno.

  4. Demo Steps:

      Normal: Shake the device gently -> OLED shows "ANALYZING" -> Returns to "MONITORING" (False Alarm).

      Fall: Hit the device against a soft surface (Impact) -> Keep it still for 5 seconds -> ALARM TRIGGERS.

      Reset: Hold the button for 2 seconds to stop the alarm.



