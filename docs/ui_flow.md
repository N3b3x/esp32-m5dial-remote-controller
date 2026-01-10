# M5Dial Remote Controller UI Flow

This project targets the M5Stack Dial (1.28" round IPS + touch + encoder). The UI is intentionally built around a **round display**: the Landing page uses a circular “visible area” and avoids relying on the square corners.

## Controls

- **Encoder rotate**
  - Landing: move between main menu items
  - Settings: move selection / edit values (when value-edit mode is active)
  - Terminal: scroll (when encoder-scroll is ON)
- **BtnA click** (and/or **tap**)
  - Landing: open the currently selected menu item
  - Settings: select/toggle/edit/save/back depending on focus
  - Bounds: run bounds
  - Live Counter: start/stop
  - Terminal: toggle encoder-scroll mode (tap the top bar also works)
- **Touch drag**
  - Terminal: scroll log history

## Page Map

Landing (Main Menu)
- Displays a slow animated background (color waves) **only here**.
- Shows a single selected item in the center:
  - Icon (RGB565 asset with transparent chroma key)
  - Label underneath
- Shows a small status line near the bottom (cycle/state) when status has been received.
- Shows small dots near the bottom edge as the “carousel indicator”.

Settings
- Editing is done on a **local edit buffer**.
- **Back**: discards edits and returns to Landing.
- **Save**: persists to NVS and returns to Landing; also sends `ConfigSet` to the test unit.

Bounds
- Single action page.
- Press/tap the button to send `RunBoundsFinding`.
- Shows a short “running…” state.

Live Counter
- Shows the latest cycle count.
- Single action button: Start/Stop depending on last known device state.

Terminal
- Ring buffer of timestamped lines.
- Touch drag scrolls history.
- Encoder-scroll mode can be toggled from the top bar.

## Rendering Notes (Round Screen)

- Landing uses a circle-clip cache (per-scanline extents) so the animated background is drawn only within the visible round area.
- Internal pages keep a plain background for readability.

## Where This Lives

- UI logic: [main/ui/ui_controller.cpp](../main/ui/ui_controller.cpp)
- Menu icons (RGB565 arrays): [main/ui/assets/menu_icons.hpp](../main/ui/assets/menu_icons.hpp)
- Icon generator (no external deps): [tools/generate_menu_icons.py](../tools/generate_menu_icons.py)
