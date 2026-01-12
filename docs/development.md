# Development Guide

## Build Environment
This project uses **PlatformIO** or the **ESP-IDF** toolchain.

### Prerequisites
*   VS Code with PlatformIO extension (Recommended)
*   or ESP-IDF v5.x installed manually.

### Dependencies
Managed via `platformio.ini` or `idf_component.yml`.
*   **M5Unified**: Hardware abstraction.
*   **M5GFX**: Graphics library.

## Building and Flashing

### Using PlatformIO
1.  Open the project in VS Code.
2.  Select the `M5Dial` environment.
3.  Click **Build** (checkmark icon).
4.  Connect M5Dial via USB.
5.  Click **Upload** (arrow icon).

### Using ESP-IDF
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## Configuration
The file `main/config.hpp` contains compile-time configuration:
*   `TEST_UNIT_MAC_`: The MAC address of the target Fatigue Test Unit. **Update this to match your receiver.**
*   `DIAL_ENCODER_PIN_*`: GPIO definitions for the rotary encoder (if custom hardware).

## Adding New Features

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

## Debugging
*   Serial output is enabled at 115200 baud.
*   Use `ESP_LOGI`, `ESP_LOGE` macros for logging.
*   The device also features an on-screen **Terminal** page for viewing logs without a PC.
