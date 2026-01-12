# M5Dial Remote Controller

**A wireless remote control interface for Fatigue Testing Units, built on the M5Stack Dial.**

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)

## Overview
This repository contains the firmware for the **M5Dial Remote Controller**. It provides a modern, touch-and-dial interface to wirelessly control and monitor industrial fatigue testing machines. It communicates using the robust ESP-NOW protocol, ensuring low-latency and reliable operation without the need for a WiFi router.

**Key Features:**
*   **Intuitive UI**: Circular menus optimized for the M5Dial's form factor.
*   **Wireless Control**: Start, Stop, Pause, and Monitor tests remotely.
*   **Live Dashboard**: Real-time visualization of cycle counts, speed, and status.
*   **Configuration**: Full control over test parameters (Velocity, Acceleration, Dwell, Bounds).
*   **Bounds Finding**: Automated wizard to detect machine range of motion.
*   **Terminal**: On-device log viewer for debugging.

## Documentation
Comprehensive documentation is available in the `docs/` directory:

*   [**System Architecture**](docs/architecture.md): Overview of hardware and software design.
*   [**User Guide**](docs/user_guide.md): How to operate the device.
*   [**Communication Protocol**](docs/communication.md): Details of the ESP-NOW packets.
*   [**UI System**](docs/ui_system.md): Deep dive into the user interface implementation.
*   [**Development Guide**](docs/development.md): How to build, flash, and modify the code.

## Quick Start
1.  **Clone** the repository.
2.  **Open** in VS Code with PlatformIO.
3.  **Update Config**: Edit `main/config.hpp` and set `TEST_UNIT_MAC_` to your target machine's MAC address.
4.  **Build & Flash**: Upload to your M5Stack Dial.

## Hardware
*   [M5Stack Dial](https://docs.m5stack.com/en/core/M5Dial) (ESP32-S3 Smart Rotary Screen)

## License
This project is open-source. See the LICENSE file for details.
