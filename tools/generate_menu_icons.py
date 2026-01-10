#!/usr/bin/env python3
"""Generate polished RGB565 icon assets for the M5Dial UI.

No external dependencies (no Pillow). Icons are geometric rasterizations
with anti-aliasing, gradients, and stylized designs.

Output: main/ui/assets/menu_icons.hpp
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Color:
    r: int
    g: int
    b: int

    def clamp(self) -> "Color":
        return Color(
            max(0, min(255, self.r)),
            max(0, min(255, self.g)),
            max(0, min(255, self.b)),
        )

    def to_rgb565(self) -> int:
        c = self.clamp()
        r5 = (c.r * 31 + 127) // 255
        g6 = (c.g * 63 + 127) // 255
        b5 = (c.b * 31 + 127) // 255
        return (r5 << 11) | (g6 << 5) | b5

    def lerp(self, other: "Color", t: float) -> "Color":
        """Linear interpolation between colors."""
        t = max(0.0, min(1.0, t))
        return Color(
            int(self.r + (other.r - self.r) * t),
            int(self.g + (other.g - self.g) * t),
            int(self.b + (other.b - self.b) * t),
        )


def blend_over(dst: Color, src: Color, alpha: float) -> Color:
    """Alpha-blend src over dst."""
    a = max(0.0, min(1.0, alpha))
    inv = 1.0 - a
    return Color(
        int(dst.r * inv + src.r * a),
        int(dst.g * inv + src.g * a),
        int(dst.b * inv + src.b * a),
    )


class Canvas:
    def __init__(self, w: int, h: int, bg: Color) -> None:
        self.w = w
        self.h = h
        self.bg = bg
        self.px = [bg for _ in range(w * h)]

    def _idx(self, x: int, y: int) -> int:
        return y * self.w + x

    def set(self, x: int, y: int, c: Color) -> None:
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[self._idx(x, y)] = c

    def get(self, x: int, y: int) -> Color:
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.px[self._idx(x, y)]
        return self.bg

    def seta(self, x: int, y: int, c: Color, a: float) -> None:
        if 0 <= x < self.w and 0 <= y < self.h:
            idx = self._idx(x, y)
            self.px[idx] = blend_over(self.px[idx], c, a)

    def line_aa(self, x0: float, y0: float, x1: float, y1: float, c: Color, width: float = 1.0) -> None:
        """Anti-aliased line with variable thickness."""
        dx = x1 - x0
        dy = y1 - y0
        dist = math.sqrt(dx * dx + dy * dy)
        if dist < 0.01:
            return
        
        steps = int(dist * 2) + 1
        hw = width / 2.0
        
        for i in range(steps + 1):
            t = i / steps
            px = x0 + dx * t
            py = y0 + dy * t
            
            # Draw anti-aliased disc at each point
            for iy in range(int(py - hw - 1), int(py + hw + 2)):
                for ix in range(int(px - hw - 1), int(px + hw + 2)):
                    d = math.sqrt((ix - px) ** 2 + (iy - py) ** 2)
                    if d <= hw + 1:
                        a = max(0.0, min(1.0, hw + 1 - d))
                        self.seta(ix, iy, c, a)

    def line(self, x0: int, y0: int, x1: int, y1: int, c: Color) -> None:
        # Bresenham
        dx = abs(x1 - x0)
        dy = -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            self.set(x0, y0, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def circle(self, cx: int, cy: int, r: int, c: Color) -> None:
        x = r
        y = 0
        err = 0
        while x >= y:
            for dx, dy in (
                (x, y), (y, x), (-y, x), (-x, y),
                (-x, -y), (-y, -x), (y, -x), (x, -y),
            ):
                self.set(cx + dx, cy + dy, c)
            y += 1
            err += 1 + 2 * y
            if 2 * (err - x) + 1 > 0:
                x -= 1
                err += 1 - 2 * x

    def fill_circle(self, cx: int, cy: int, r: int, c: Color) -> None:
        rr = r * r
        for y in range(cy - r, cy + r + 1):
            dy = y - cy
            dy2 = dy * dy
            if dy2 > rr:
                continue
            dx = int(math.sqrt(rr - dy2))
            x0 = cx - dx
            x1 = cx + dx
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def fill_circle_aa(self, cx: float, cy: float, r: float, c: Color) -> None:
        """Anti-aliased filled circle."""
        for y in range(int(cy - r - 2), int(cy + r + 3)):
            for x in range(int(cx - r - 2), int(cx + r + 3)):
                d = math.sqrt((x - cx) ** 2 + (y - cy) ** 2)
                if d <= r - 0.5:
                    self.set(x, y, c)
                elif d <= r + 0.5:
                    a = r + 0.5 - d
                    self.seta(x, y, c, a)

    def fill_circle_gradient(self, cx: float, cy: float, r: float, c_inner: Color, c_outer: Color) -> None:
        """Gradient-filled anti-aliased circle."""
        for y in range(int(cy - r - 2), int(cy + r + 3)):
            for x in range(int(cx - r - 2), int(cx + r + 3)):
                d = math.sqrt((x - cx) ** 2 + (y - cy) ** 2)
                if d <= r + 0.5:
                    t = min(1.0, d / r)
                    gc = c_inner.lerp(c_outer, t)
                    if d <= r - 0.5:
                        self.set(x, y, gc)
                    else:
                        a = r + 0.5 - d
                        self.seta(x, y, gc, a)

    def ring_aa(self, cx: float, cy: float, r: float, thickness: float, c: Color) -> None:
        """Anti-aliased ring (hollow circle)."""
        inner = r - thickness / 2
        outer = r + thickness / 2
        for y in range(int(cy - outer - 2), int(cy + outer + 3)):
            for x in range(int(cx - outer - 2), int(cx + outer + 3)):
                d = math.sqrt((x - cx) ** 2 + (y - cy) ** 2)
                if inner - 0.5 <= d <= outer + 0.5:
                    # Compute alpha based on distance from ring edges
                    a_outer = max(0, min(1, outer + 0.5 - d))
                    a_inner = max(0, min(1, d - inner + 0.5))
                    a = min(a_outer, a_inner)
                    self.seta(x, y, c, a)

    def ring(self, cx: int, cy: int, r0: int, r1: int, c: Color) -> None:
        # fill circle r1 then punch with bg by setting inner to bg later (handled by caller)
        self.fill_circle(cx, cy, r1, c)
        # Caller should clear inner if desired.


def icon_settings(w: int, h: int, bg: Color, palette: dict) -> Canvas:
    """Gear icon with gradient and anti-aliasing."""
    cv = Canvas(w, h, bg)
    cx, cy = w / 2, h / 2
    fg = palette["fg"]
    accent = palette["accent"]
    
    # Outer gear body with gradient
    cv.fill_circle_gradient(cx, cy, 24, accent, fg)
    
    # Clear inner hole
    cv.fill_circle_aa(cx, cy, 12, bg)
    
    # Gear teeth
    teeth = 8
    for i in range(teeth):
        ang = (2 * math.pi * i) / teeth
        tx = cx + math.cos(ang) * 22
        ty = cy + math.sin(ang) * 22
        cv.fill_circle_aa(tx, ty, 6, fg)
    
    # Center knob with highlight
    cv.fill_circle_gradient(cx, cy, 8, Color(255, 255, 255), accent)
    cv.fill_circle_aa(cx, cy, 4, bg)
    
    return cv


def icon_bounds(w: int, h: int, bg: Color, palette: dict) -> Canvas:
    """Target/crosshair icon with anti-aliasing."""
    cv = Canvas(w, h, bg)
    cx, cy = w / 2, h / 2
    fg = palette["fg"]
    accent = palette["accent"]
    red = palette.get("red", Color(255, 80, 80))
    
    # Outer ring
    cv.ring_aa(cx, cy, 24, 3, fg)
    
    # Middle ring
    cv.ring_aa(cx, cy, 16, 2, accent)
    
    # Crosshairs with gap in center
    cv.line_aa(cx - 28, cy, cx - 10, cy, fg, 2.5)
    cv.line_aa(cx + 10, cy, cx + 28, cy, fg, 2.5)
    cv.line_aa(cx, cy - 28, cx, cy - 10, fg, 2.5)
    cv.line_aa(cx, cy + 10, cx, cy + 28, fg, 2.5)
    
    # Center dot (red for targeting)
    cv.fill_circle_gradient(cx, cy, 5, Color(255, 150, 150), red)
    
    return cv


def icon_live(w: int, h: int, bg: Color, palette: dict) -> Canvas:
    """Live/play indicator with circular motion suggestion."""
    cv = Canvas(w, h, bg)
    cx, cy = w / 2, h / 2
    fg = palette["fg"]
    accent = palette["accent"]
    green = palette.get("green", Color(80, 220, 120))
    
    # Circular arc suggesting motion
    for i in range(180):
        ang = math.radians(i - 45)
        x = cx + math.cos(ang) * 22
        y = cy + math.sin(ang) * 22
        fade = 0.3 + 0.7 * (i / 180)
        cv.fill_circle_aa(x, y, 2.5 * fade, fg.lerp(accent, fade))
    
    # Arrow head at end of arc
    ang_end = math.radians(135)
    ax = cx + math.cos(ang_end) * 22
    ay = cy + math.sin(ang_end) * 22
    cv.line_aa(ax, ay, ax + 8, ay - 4, fg, 3)
    cv.line_aa(ax, ay, ax + 4, ay + 8, fg, 3)
    
    # Play triangle in center (green for "go")
    tri_cx = cx
    tri_cy = cy
    
    # Draw filled triangle
    for y in range(int(tri_cy - 10), int(tri_cy + 11)):
        row_t = (y - (tri_cy - 10)) / 20.0
        hw = 7 * min(row_t * 2, 2 - row_t * 2)  # Diamond shape rotated
        x_start = tri_cx - 5 + row_t * 10
        for x in range(int(x_start - hw), int(x_start + hw + 1)):
            # Distance to triangle bounds
            dist_to_edge = hw - abs(x - x_start)
            if dist_to_edge > 0.5:
                c = green.lerp(Color(150, 255, 180), 0.3)
                cv.set(int(x), y, c)
            elif dist_to_edge > -0.5:
                cv.seta(int(x), y, green, dist_to_edge + 0.5)
    
    return cv


def icon_terminal(w: int, h: int, bg: Color, palette: dict) -> Canvas:
    """Terminal/console icon."""
    cv = Canvas(w, h, bg)
    cx, cy = w / 2, h / 2
    fg = palette["fg"]
    accent = palette["accent"]
    
    # Rounded rectangle terminal window
    rect_w, rect_h = 44, 36
    rx, ry = cx - rect_w / 2, cy - rect_h / 2
    
    # Terminal body (rounded corners via circles)
    corner_r = 6
    for y in range(int(ry + corner_r), int(ry + rect_h - corner_r)):
        for x in range(int(rx), int(rx + rect_w)):
            cv.set(x, y, fg)
    for x in range(int(rx + corner_r), int(rx + rect_w - corner_r)):
        for y in range(int(ry), int(ry + rect_h)):
            cv.set(x, y, fg)
    cv.fill_circle_aa(rx + corner_r, ry + corner_r, corner_r, fg)
    cv.fill_circle_aa(rx + rect_w - corner_r, ry + corner_r, corner_r, fg)
    cv.fill_circle_aa(rx + corner_r, ry + rect_h - corner_r, corner_r, fg)
    cv.fill_circle_aa(rx + rect_w - corner_r, ry + rect_h - corner_r, corner_r, fg)
    
    # Inner dark area
    inner_margin = 4
    for y in range(int(ry + inner_margin + 6), int(ry + rect_h - inner_margin)):
        for x in range(int(rx + inner_margin), int(rx + rect_w - inner_margin)):
            cv.set(x, y, bg)
    
    # Title bar with dots
    cv.fill_circle_aa(rx + 10, ry + 6, 2.5, Color(255, 95, 86))   # Red
    cv.fill_circle_aa(rx + 18, ry + 6, 2.5, Color(255, 189, 46))  # Yellow
    cv.fill_circle_aa(rx + 26, ry + 6, 2.5, Color(39, 201, 63))   # Green
    
    # Prompt chevron >_
    prompt_y = ry + 18
    cv.line_aa(rx + 8, prompt_y, rx + 14, prompt_y + 5, accent, 2)
    cv.line_aa(rx + 8, prompt_y + 10, rx + 14, prompt_y + 5, accent, 2)
    
    # Cursor line
    cv.line_aa(rx + 18, prompt_y + 8, rx + 22, prompt_y + 8, fg, 2)
    
    # Text lines (fading)
    for i, alpha in enumerate([0.6, 0.4, 0.25]):
        ly = prompt_y + 14 + i * 5
        lw = 20 - i * 4
        c = fg.lerp(bg, 1.0 - alpha)
        cv.line_aa(rx + 8, ly, rx + 8 + lw, ly, c, 1.5)
    
    return cv


def icon_home(w: int, h: int, bg: Color, palette: dict) -> Canvas:
    """HF monogram logo in a modern style."""
    cv = Canvas(w, h, bg)
    cx, cy = w / 2, h / 2
    fg = palette["fg"]
    accent = palette["accent"]
    
    # Outer accent ring
    cv.ring_aa(cx, cy, 26, 3, accent)
    
    # Inner white ring
    cv.ring_aa(cx, cy, 22, 2, fg)
    
    # "H" - stylized
    h_left = cx - 12
    h_right = cx - 4
    # Verticals
    cv.line_aa(h_left, cy - 12, h_left, cy + 12, fg, 3)
    cv.line_aa(h_right, cy - 12, h_right, cy + 12, fg, 3)
    # Horizontal
    cv.line_aa(h_left, cy, h_right, cy, fg, 3)
    
    # "F" - stylized
    f_left = cx + 4
    # Vertical
    cv.line_aa(f_left, cy - 12, f_left, cy + 12, fg, 3)
    # Top horizontal
    cv.line_aa(f_left, cy - 12, f_left + 10, cy - 12, fg, 3)
    # Middle horizontal (shorter)
    cv.line_aa(f_left, cy - 2, f_left + 7, cy - 2, fg, 3)
    
    return cv


def write_header(out_path: Path, icons: dict[str, Canvas], bg565: int) -> None:
    def fmt_u16(v: int) -> str:
        return f"0x{v:04x}"  # lower-case hex

    lines: list[str] = []
    lines.append("#pragma once")
    lines.append("\n// Auto-generated by tools/generate_menu_icons.py")
    lines.append("// Icons are RGB565 with anti-aliasing and gradient effects.")
    lines.append("// Chroma-key transparent background for compositing.")
    lines.append("\n#include <cstdint>")
    lines.append("\nnamespace ui::assets {\n")
    lines.append(f"static constexpr uint16_t kTransparent565 = {fmt_u16(bg565)};\n")

    for name, cv in icons.items():
        arr_name = f"kIcon_{name}".replace("-", "_")
        lines.append(f"static constexpr int {arr_name}_W = {cv.w};")
        lines.append(f"static constexpr int {arr_name}_H = {cv.h};")
        lines.append(f"static const uint16_t {arr_name}[{cv.w * cv.h}] = {{")

        # Write row-major; 12 values per line.
        row: list[str] = []
        for i, p in enumerate(cv.px):
            row.append(fmt_u16(p.to_rgb565()))
            if len(row) == 12:
                lines.append("  " + ", ".join(row) + ",")
                row = []
        if row:
            lines.append("  " + ", ".join(row) + ",")
        lines.append("};\n")

    lines.append("} // namespace ui::assets\n")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    w = 64
    h = 64
    bg = Color(0, 0, 0)  # chroma-key transparent
    
    # Color palette for a modern, cohesive look
    palette = {
        "fg": Color(240, 240, 245),      # Near-white
        "accent": Color(100, 160, 255),  # Soft blue accent
        "red": Color(255, 90, 90),       # For bounds target
        "green": Color(80, 220, 120),    # For live/play
    }

    icons: dict[str, Canvas] = {
        "home": icon_home(w, h, bg, palette),
        "settings": icon_settings(w, h, bg, palette),
        "bounds": icon_bounds(w, h, bg, palette),
        "live": icon_live(w, h, bg, palette),
        "terminal": icon_terminal(w, h, bg, palette),
    }

    # Output into: main/ui/assets/
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    out_path = project_root / "main" / "ui" / "assets" / "menu_icons.hpp"
    write_header(out_path, icons, bg.to_rgb565())
    print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
