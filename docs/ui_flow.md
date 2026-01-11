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
- **BtnA long-press**
  - Settings (value editor): cycle edit step size for numeric values
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
- Incoming config messages normally refresh the displayed values, but **won’t overwrite local edits** while changes are pending.
- Leaving Settings with unsent changes prompts for what to do:
  - **Send**: transmit all changes to the fatigue-test unit (only if connected).
  - **Resync**: discard local edits and reload from the last known machine config.
- On first connect (or reconnect), the UI **always resyncs from the machine**. Any offline edits are treated as stale and will be discarded on the first config response.

### Value editor behavior

- Rotation applies the current step size.
- Long-press cycles step size: `{0.1, 1, 10}`.
- Values are rounded and displayed with **at most one decimal place**.
- Dwell time is edited/displayed in **seconds** with **0.5s increments** (stored/transmitted in milliseconds).

Bounds
- Action page with on-screen controls: **Start**, **Stop**, **Back**.
- **Start** sends the bounds-finding command; the UI does not show “running” until the fatigue-test unit ACKs.
- While running, the page shows progress feedback and allows **Stop**.
- After completion, the page displays bounds using a crosshair-style visualization (negative/positive) sized for a 240×240 round display.

Pairing / Discovery
- The transport supports secure discovery/pairing (approved peers stored in NVS).
- If no approved peer (or configured peer) exists, outbound control messages may be blocked until paired.

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
