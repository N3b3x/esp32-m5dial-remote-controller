# System Architecture

## Overview
The M5Dial Remote Controller is a specialized input/output device designed to control and monitor a fatigue testing unit wirelessly. It leverages the M5Stack Dial hardware (ESP32-S3) and communicates via the ESP-NOW protocol for low-latency, connectionless data transfer.

## Hardware Platform
*   **Device:** M5Stack Dial
*   **MCU:** ESP32-S3
*   **Display:** 1.28" Round Touch Screen (GC9A01 Driver)
*   **Input:** Rotary Encoder (Ring), Touch Screen, Physical Button (under encoder)
*   **Power:** Built-in Battery / USB-C

## Software Stack
The firmware is built using the ESP-IDF framework with Arduino components bridged via `M5Unified`.

### Core Components
1.  **M5Unified**: Hardware Abstraction Layer (HAL) for display, power management, IMU, and inputs.
2.  **M5GFX**: Graphics library used for high-performance rendering.
3.  **ESP-NOW**: Wireless communication protocol.
4.  **FreeRTOS**: Real-time operating system managing tasks (Main App, Protocol handling).

### Application Structure
*   **`main.cpp`**: Entry point. Initializes NVS, M5Unified, ESP-NOW, and the UI Controller.
*   **`ui/`**: Contains the User Interface logic.
    *   `UiController`: The central class managing application state, rendering, and input processing.
    *   Uses a "Page" system (Landing, Settings, LiveCounter, etc.) to manage screen context.
    *   Implements a double-buffered rendering loop using `LGFX_Sprite` to prevent flickering.
*   **`protocol/`**: Handles wireless communication.
    *   `espnow_protocol`: Manages packet encoding/decoding, CRCs, and transmission.
    *   `fatigue_protocol`: Defines specific payload structures for the fatigue tester application.
*   **`settings`**: Manages persistent configuration (though currently running in-memory for some aspects).

## Data Flow
1.  **Input**: User rotates the dial or touches the screen -> `UiController::handleInputs_` processes these events.
2.  **Logic**: `UiController::Tick` updates the state machine (e.g., navigating menus, adjusting values).
3.  **Network**:
    *   **Tx**: UI actions trigger `espnow::Send...` functions.
    *   **Rx**: ESP-NOW callback pushes events to a FreeRTOS queue (`proto_queue`).
    *   **Processing**: `UiController` consumes the queue in its main loop to update the UI (e.g., showing connection status, updating live counters).
4.  **Output**: `UiController::draw_` renders the current state to the display buffer, which is pushed to the screen.

## Directory Structure
```
/workspace
├── components/         # External libraries (M5Unified, M5Dial, etc.)
├── docs/              # Documentation
├── main/              # Application Source
│   ├── protocol/      # Network logic
│   ├── ui/           # User Interface logic & assets
│   ├── config.hpp     # Pin definitions and global constants
│   ├── main.cpp       # Application entry point
│   └── settings.*     # Configuration management
└── tools/             # Utility scripts
```
