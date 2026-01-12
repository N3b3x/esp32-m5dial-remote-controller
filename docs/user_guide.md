# User Guide

## Getting Started
1.  Power on the M5Dial Remote Controller.
2.  The device will boot and attempt to connect to the configured Fatigue Test Unit via ESP-NOW.
3.  The **Landing Page** (Main Menu) will appear.

## Navigation
*   **Rotate Dial**: Scroll through menu items or adjust values.
*   **Press Dial**: Select an item or confirm an action.
*   **Touch Screen**: You can also tap icons or list items to select them. Swipe right to go back in most menus.

## Main Menu (Landing Page)
Rotate the dial to highlight a mode and press to select:
*   **Fatigue Test**: Go to the live monitoring and control dashboard.
*   **Bounds**: Run the automated bounds finding procedure.
*   **Settings**: Configure test parameters and device settings.
*   **Terminal**: View system logs.

## Running a Fatigue Test
1.  **Configure**: Go to `Settings` -> `Fatigue Test` to set your desired parameters (Cycles, VMAX, AMAX).
2.  **Monitor**: Go to the `Fatigue Test` (Live Counter) page.
3.  **Start**: Press the dial. A popup will appear. Select `START`.
    *   The unit will begin the test.
    *   Real-time cycle count and status are displayed.
4.  **Pause/Stop**: Press the dial during a test to open the action menu. Select `PAUSE` or `STOP`.
5.  **Quick Settings**: Long-press the dial on the Live Counter page to quickly adjust speed/cycles without stopping the test (if supported) or to prep for the next run.

## Bounds Finding
*   Go to the `Bounds` page.
*   Press the dial to `START` the search.
*   The machine will slowly move to find physical end-stops using StallGuard or Encoder feedback.
*   Upon completion, the found range (Min/Max degrees) is displayed.

## Editing Settings
1.  Navigate to `Settings`.
2.  Select a category (e.g., `Fatigue Test`).
3.  Select a parameter (e.g., `Target Cycles`).
4.  **Value Editor**:
    *   Rotate to change the value.
    *   **Long-Press** the dial to change the increment step (e.g., x1, x10, x100).
    *   Press once to `Save` or `Back`.
5.  When leaving the Settings menu, you will be prompted to `SEND` changes to the machine.

## Troubleshooting
*   **"Connecting..."**: The remote cannot reach the test unit. Ensure the test unit is powered on and within range.
*   **Red LED Ring**: Indicates an error state on the machine. Check the Terminal or Machine logs.
