# UI System Architecture

The User Interface is built on top of `M5GFX` and provides a responsive, flick-free experience using double-buffering.

## Architecture

The `ui::UiController` class is the heart of the interface. It follows a loop-based architecture:

1.  **Tick()**: Called every ~16ms.
    *   Polls inputs (Touch, Encoder).
    *   Processes network events.
    *   Updates logic (animations, state transitions).
    *   Triggers redraw (`draw_()`).

2.  **Rendering**:
    *   Uses an `LGFX_Sprite` as a back-buffer (`canvas_`).
    *   All drawing operations happen on the sprite.
    *   The sprite is pushed to the physical display at the end of the frame.
    *   This eliminates tearing and flickering.

## Input Handling

*   **Rotary Encoder**: Used for scrolling lists, adjusting values, and navigating the circular menu.
    *   Handled by `EC11Encoder` wrapper.
    *   `onRotate_()` callback updates state based on the current page.
*   **Touch Screen**: Used for selection, swiping, and confirmation.
    *   Supports tap, drag, and swipe gestures.
    *   `onTouchClick_`, `onTouchDrag_`, `onSwipe_` handlers.
*   **Physical Button**: The button under the screen acts as "Select" or "Enter".

## Page System

The UI is divided into distinct "Pages" (`ui::Page` enum):

| Page | Description |
| :--- | :--- |
| `Landing` | Circular carousel main menu. |
| `Settings` | Hierarchical list for configuring parameters. |
| `LiveCounter` | Real-time dashboard for the Fatigue Test. |
| `Bounds` | Control screen for the Bounds Finding process. |
| `Terminal` | Debug log viewer. |

## Key UI Components

### Circular Menu
*   Used on the `Landing` page.
*   Rotates icons around the circular display.
*   Smoothly animates transitions using cubic easing.

### Settings Editor
*   **List View**: Scrollable list of settings.
*   **Value Editor**: Dedicated full-screen editor for modifying a single value.
    *   Supports `float`, `int`, `bool` types.
    *   Rotary encoder changes values; long-press changes step size (e.g., 0.1 -> 1.0 -> 10.0).

### Live Popup
*   Context-aware overlay on the `LiveCounter` page.
*   Provides quick actions (Start, Stop, Pause) without leaving the monitoring view.
*   Activated by clicking/tapping during a test.

## Theming
*   Colors and icons are defined in `ui_theme.hpp` and `assets/`.
*   Uses a consistent color scheme (Green for start, Red for stop/error, Orange for warnings).
