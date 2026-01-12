# Development Guide

This guide details how to set up your development environment, build the firmware, and flash it to the M5Stack Dial using the provided helper scripts.

## 1. Getting the Code

Clone the repository and its submodules. The project relies on submodules for shared components (like M5Unified).

```bash
# Clone the repository
git clone --recursive <repository-url>
cd <repository-name>

# If you already cloned without --recursive, run:
git submodule update --init --recursive
```

## 2. Prerequisites

The build system is based on **ESP-IDF** (Espressif IoT Development Framework).

1.  **Install ESP-IDF v5.x**: Follow the official [Espressif Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html).
2.  **Activate the Environment**: Before running any scripts, you must export the IDF environment variables in your terminal.
    ```bash
    # Linux / macOS
    . $HOME/esp/esp-idf/export.sh

    # Windows (PowerShell)
    $HOME/esp/esp-idf/export.ps1
    ```

## 3. Building the Firmware

The project uses a custom `CMakeLists.txt` that requires specific variables (`APP_TYPE`, `BUILD_TYPE`). **You must use the provided build script** rather than running `idf.py build` directly.

### The Build Script
Use `scripts/build_app.sh` to compile the project.

**Usage:**
```bash
./scripts/build_app.sh [APP_TYPE] [BUILD_TYPE]
```

*   `APP_TYPE`: The application configuration to build (Default: `m5dial_remote_controller`).
*   `BUILD_TYPE`: `Release` (optimized) or `Debug` (with symbols/logging). (Default: `Release`).

**Examples:**
```bash
# Build the default remote controller (Release)
./scripts/build_app.sh

# Build in Debug mode
./scripts/build_app.sh m5dial_remote_controller Debug

# List available app types
./scripts/build_app.sh list
```

## 4. Flashing and Monitoring

Use the `scripts/flash_app.sh` script to flash the firmware and monitor the serial output.

**Usage:**
```bash
./scripts/flash_app.sh [APP_TYPE] [BUILD_TYPE] [PORT]
```

*   `APP_TYPE`: Must match the built application.
*   `BUILD_TYPE`: Must match the build type.
*   `PORT`: The serial port of the M5Dial (e.g., `/dev/ttyACM0` on Linux, `COMx` on Windows). (Default: `/dev/ttyACM0`).

**Examples:**
```bash
# Flash the default build to default port
./scripts/flash_app.sh

# Flash a specific build to a specific port
./scripts/flash_app.sh m5dial_remote_controller Release /dev/ttyUSB0
```

> **Note**: This script runs `idf.py flash monitor`. To exit the serial monitor, press `Ctrl+]`.

## 5. Configuration

The file `main/config.hpp` contains compile-time configuration:
*   `TEST_UNIT_MAC_`: The MAC address of the target Fatigue Test Unit. **Update this to match your receiver.**
*   `DIAL_ENCODER_PIN_*`: GPIO definitions for the rotary encoder (if custom hardware).

## 6. Adding New Features

### Adding a New Setting
1.  **Protocol**: Update `ConfigPayload` in `main/protocol/fatigue_protocol.hpp`.
2.  **Settings Class**: Update `Settings` struct in `main/settings.hpp`.
3.  **UI**:
    *   Add the item to `ui/ui_controller.cpp` in `drawSettings_`.
    *   Handle the logic in `handleSettingsValueEdit_`.

### Adding a New Page
1.  Add entry to `Page` enum in `ui/ui_controller.hpp`.
2.  Add drawing logic `drawMyNewPage_` in `ui_controller.cpp`.
3.  Add dispatch logic in `draw_`.
4.  Add navigation entry (e.g., in `kMenuItems_` for the landing page).

## 7. Debugging
*   Serial output is enabled at 115200 baud.
*   Use `ESP_LOGI`, `ESP_LOGE` macros for logging.
*   The device also features an on-screen **Terminal** page for viewing logs without a PC.
