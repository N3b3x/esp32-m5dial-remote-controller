#!/usr/bin/env python3
"""
Generate high-quality circular menu icons for M5Dial UI.

Produces 42x42 RGB565 icons with colored circular backgrounds and
crisp, sharp symbols optimized for IPS displays.

Features:
- Multi-sample anti-aliasing (4x4 subpixel) for smooth edges
- Improved symbol designs with better definition
- Optimized line widths for small display sizes
- Enhanced contrast for visibility

Usage:
    python generate_circular_icons.py > ../main/ui/assets/circular_icons.hpp
"""

import math
from dataclasses import dataclass
from typing import List, Tuple, Callable

# Icon dimensions matching factory demo
ICON_SIZE = 42
ICON_RADIUS = 21  # Radius of circular background

# Anti-aliasing samples per pixel (4x4 = 16 samples)
AA_SAMPLES = 4

# Colors matching factory demo icon palette (RGB565)
ICON_COLORS = {
    'red':      0xFD5C,   # Settings/gear
    'blue':     0x577E,   # Bounds/target
    'green':    0x03A9,   # Live Counter/play
    'teal':     0x1AA1,   # Terminal/console
    'orange':   0xEB84,   # Brightness
    'mint':     0x04A2,   # WiFi
    'cyan':     0x008C,   # Info
    'gray':     0x5D7B,   # More/menu
}

# Selector/label color (cream)
SELECTOR_COLOR = 0xF3E9  # 0xF3E9D2

# RGB565 conversion utilities
def rgb_to_565(r: int, g: int, b: int) -> int:
    """Convert 8-bit RGB to RGB565."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def rgb565_to_rgb(color: int) -> Tuple[int, int, int]:
    """Convert RGB565 to 8-bit RGB."""
    r = ((color >> 11) & 0x1F) << 3
    g = ((color >> 5) & 0x3F) << 2
    b = (color & 0x1F) << 3
    return (r, g, b)

def blend_rgb(fg: Tuple[int, int, int], bg: Tuple[int, int, int], alpha: float) -> Tuple[int, int, int]:
    """Alpha blend two RGB colors."""
    r = int(fg[0] * alpha + bg[0] * (1 - alpha))
    g = int(fg[1] * alpha + bg[1] * (1 - alpha))
    b = int(fg[2] * alpha + bg[2] * (1 - alpha))
    return (r, g, b)

def blend_565(fg: int, bg: int, alpha: float) -> int:
    """Alpha blend two RGB565 colors."""
    fg_rgb = rgb565_to_rgb(fg)
    bg_rgb = rgb565_to_rgb(bg)
    result_rgb = blend_rgb(fg_rgb, bg_rgb, alpha)
    return rgb_to_565(*result_rgb)

def distance(x1: float, y1: float, x2: float, y2: float) -> float:
    """Euclidean distance."""
    return math.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2)

def clamp(val: float, min_val: float = 0.0, max_val: float = 1.0) -> float:
    """Clamp value to range."""
    return max(min_val, min(max_val, val))

@dataclass
class IconDef:
    """Definition for generating an icon."""
    name: str
    bg_color: int  # RGB565
    symbol: str    # Type of symbol to draw

def sample_pixel_aa(x: int, y: int, size: int, shape_func: Callable[[float, float], float]) -> float:
    """
    Sample a pixel with multi-sample anti-aliasing.
    shape_func returns 0.0-1.0 coverage for a point.
    """
    total = 0.0
    step = 1.0 / AA_SAMPLES
    offset = step / 2
    
    for sy in range(AA_SAMPLES):
        for sx in range(AA_SAMPLES):
            sample_x = x + offset + sx * step
            sample_y = y + offset + sy * step
            total += shape_func(sample_x, sample_y)
    
    return total / (AA_SAMPLES * AA_SAMPLES)

def generate_circle_background(size: int, color: int, transparent: int = 0x0000) -> List[int]:
    """Generate a filled circle on transparent background with AA."""
    pixels = []
    cx = cy = size / 2 - 0.5
    radius = size / 2 - 0.5
    
    def circle_coverage(x: float, y: float) -> float:
        dist = distance(x, y, cx, cy)
        if dist <= radius - 0.5:
            return 1.0
        elif dist >= radius + 0.5:
            return 0.0
        else:
            return clamp(radius + 0.5 - dist)
    
    for y in range(size):
        for x in range(size):
            coverage = sample_pixel_aa(x, y, size, circle_coverage)
            if coverage > 0.01:
                pixels.append(blend_565(color, transparent, coverage))
            else:
                pixels.append(transparent)
    
    return pixels

def draw_gear_symbol(pixels: List[int], size: int, fg_color: int) -> None:
    """Draw a crisp gear/settings symbol with clean edges."""
    cx = cy = size / 2 - 0.5
    
    # Gear parameters - refined for crispness
    outer_r = size / 2 - 7.5    # Outer radius of gear body
    inner_r = outer_r * 0.6     # Inner radius (before teeth)
    hole_r = outer_r * 0.28     # Center hole
    num_teeth = 8
    tooth_width = 0.35          # As fraction of tooth spacing (radians)
    tooth_height = 3.5          # Tooth protrusion
    
    def gear_coverage(x: float, y: float) -> float:
        dist = distance(x, y, cx, cy)
        
        # Center hole
        if dist <= hole_r - 0.5:
            return 0.0
        if dist < hole_r + 0.5:
            return clamp(dist - hole_r + 0.5)
        
        # Calculate angle for gear teeth
        angle = math.atan2(y - cy, x - cx)
        tooth_angle = (2 * math.pi) / num_teeth
        
        # Normalize angle to [0, tooth_angle)
        normalized_angle = ((angle % tooth_angle) + tooth_angle) % tooth_angle
        center_of_tooth = tooth_angle / 2
        dist_from_center = abs(normalized_angle - center_of_tooth)
        
        # Determine effective outer radius based on tooth position
        on_tooth = dist_from_center < (tooth_width * tooth_angle / 2)
        
        if on_tooth:
            effective_outer = outer_r + tooth_height
            # Tooth sides - sharp edge
            tooth_edge_dist = (tooth_width * tooth_angle / 2) - dist_from_center
            tooth_edge_factor = clamp(tooth_edge_dist * 15)  # Sharp falloff
        else:
            effective_outer = outer_r
            tooth_edge_factor = 1.0
        
        # Main gear body
        if dist >= inner_r - 0.5 and dist <= effective_outer + 0.5:
            # Inner edge AA
            if dist < inner_r + 0.5:
                inner_factor = clamp(dist - inner_r + 0.5)
            else:
                inner_factor = 1.0
            
            # Outer edge AA
            if dist > effective_outer - 0.5:
                outer_factor = clamp(effective_outer + 0.5 - dist)
            else:
                outer_factor = 1.0
            
            return inner_factor * outer_factor * tooth_edge_factor
        
        return 0.0
    
    for y in range(size):
        for x in range(size):
            coverage = sample_pixel_aa(x, y, size, gear_coverage)
            if coverage > 0.01:
                idx = y * size + x
                pixels[idx] = blend_565(fg_color, pixels[idx], coverage * 0.95)

def draw_target_symbol(pixels: List[int], size: int, fg_color: int) -> None:
    """Draw a crisp target/crosshair symbol."""
    cx = cy = size / 2 - 0.5
    
    # Target parameters
    outer_r = size / 2 - 8
    inner_r = outer_r * 0.42
    ring_width = 2.0
    center_r = 2.5
    crosshair_width = 1.8
    
    def target_coverage(x: float, y: float) -> float:
        dist = distance(x, y, cx, cy)
        coverage = 0.0
        
        # Outer ring
        outer_dist = abs(dist - outer_r)
        if outer_dist < ring_width:
            coverage = max(coverage, clamp(1.0 - outer_dist / (ring_width * 0.5)))
        
        # Inner ring
        inner_dist = abs(dist - inner_r)
        if inner_dist < ring_width * 0.7:
            coverage = max(coverage, clamp(1.0 - inner_dist / (ring_width * 0.35)))
        
        # Center dot
        if dist < center_r:
            coverage = max(coverage, clamp(1.0 - dist / center_r * 0.7))
        
        # Crosshairs (only between rings)
        gap_inner = inner_r + ring_width + 1
        gap_outer = outer_r - ring_width - 1
        
        # Vertical crosshair
        if gap_inner < dist < gap_outer:
            x_dist = abs(x - cx)
            if x_dist < crosshair_width:
                coverage = max(coverage, clamp(1.0 - x_dist / crosshair_width))
            
            # Horizontal crosshair
            y_dist = abs(y - cy)
            if y_dist < crosshair_width:
                coverage = max(coverage, clamp(1.0 - y_dist / crosshair_width))
        
        return coverage
    
    for y in range(size):
        for x in range(size):
            coverage = sample_pixel_aa(x, y, size, target_coverage)
            if coverage > 0.01:
                idx = y * size + x
                pixels[idx] = blend_565(fg_color, pixels[idx], coverage * 0.92)

def draw_play_symbol(pixels: List[int], size: int, fg_color: int) -> None:
    """Draw a crisp play triangle symbol."""
    cx = cy = size / 2 - 0.5
    
    # Triangle parameters - slightly offset right for visual balance
    offset = 1.5
    scale = 0.42
    
    # Triangle vertices (pointing right)
    left_x = cx - size * scale * 0.35 + offset
    right_x = cx + size * scale * 0.45 + offset
    top_y = cy - size * scale * 0.4
    bottom_y = cy + size * scale * 0.4
    
    def point_in_triangle(px: float, py: float, 
                          x1: float, y1: float, 
                          x2: float, y2: float, 
                          x3: float, y3: float) -> float:
        """Check if point is in triangle, return signed distance for AA."""
        def sign(p1x, p1y, p2x, p2y, p3x, p3y):
            return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y)
        
        d1 = sign(px, py, x1, y1, x2, y2)
        d2 = sign(px, py, x2, y2, x3, y3)
        d3 = sign(px, py, x3, y3, x1, y1)
        
        has_neg = (d1 < 0) or (d2 < 0) or (d3 < 0)
        has_pos = (d1 > 0) or (d2 > 0) or (d3 > 0)
        
        inside = not (has_neg and has_pos)
        
        if inside:
            # Calculate distance to nearest edge for anti-aliasing
            def point_to_line_dist(px, py, x1, y1, x2, y2):
                dx, dy = x2 - x1, y2 - y1
                length = math.sqrt(dx * dx + dy * dy)
                if length < 0.001:
                    return distance(px, py, x1, y1)
                return abs(dy * px - dx * py + x2 * y1 - y2 * x1) / length
            
            d_to_edge = min(
                point_to_line_dist(px, py, x1, y1, x2, y2),
                point_to_line_dist(px, py, x2, y2, x3, y3),
                point_to_line_dist(px, py, x3, y3, x1, y1)
            )
            return clamp(d_to_edge + 0.5)
        return 0.0
    
    def play_coverage(x: float, y: float) -> float:
        return point_in_triangle(x, y, 
                                  left_x, top_y,
                                  left_x, bottom_y,
                                  right_x, cy)
    
    for y in range(size):
        for x in range(size):
            coverage = sample_pixel_aa(x, y, size, play_coverage)
            if coverage > 0.01:
                idx = y * size + x
                pixels[idx] = blend_565(fg_color, pixels[idx], coverage * 0.95)

def draw_terminal_symbol(pixels: List[int], size: int, fg_color: int) -> None:
    """Draw a crisp terminal/console symbol (>_ prompt)."""
    cx = cy = size / 2 - 0.5
    
    # Symbol parameters
    line_width = 2.2
    chevron_size = 6.5
    chevron_x = cx - 6
    chevron_y = cy - 1
    underscore_y = cy + 6
    underscore_x = cx + 1
    underscore_len = 9
    
    def terminal_coverage(x: float, y: float) -> float:
        coverage = 0.0
        
        # Chevron > symbol
        # Upper arm
        if y >= chevron_y - chevron_size and y <= chevron_y:
            expected_x = chevron_x + (chevron_y - y)
            dist_to_line = abs(x - expected_x)
            if dist_to_line < line_width:
                coverage = max(coverage, clamp(1.0 - dist_to_line / line_width))
        
        # Lower arm
        if y >= chevron_y and y <= chevron_y + chevron_size:
            expected_x = chevron_x + (y - chevron_y)
            dist_to_line = abs(x - expected_x)
            if dist_to_line < line_width:
                coverage = max(coverage, clamp(1.0 - dist_to_line / line_width))
        
        # Underscore _
        if underscore_x <= x <= underscore_x + underscore_len:
            dist_to_line = abs(y - underscore_y)
            if dist_to_line < line_width * 0.9:
                # Horizontal edges
                h_factor = 1.0
                if x < underscore_x + 1:
                    h_factor = clamp(x - underscore_x + 0.5)
                elif x > underscore_x + underscore_len - 1:
                    h_factor = clamp(underscore_x + underscore_len - x + 0.5)
                coverage = max(coverage, clamp(1.0 - dist_to_line / (line_width * 0.9)) * h_factor)
        
        return coverage
    
    for y in range(size):
        for x in range(size):
            coverage = sample_pixel_aa(x, y, size, terminal_coverage)
            if coverage > 0.01:
                idx = y * size + x
                pixels[idx] = blend_565(fg_color, pixels[idx], coverage * 0.92)

def draw_brightness_symbol(pixels: List[int], size: int, fg_color: int) -> None:
    """Draw a crisp sun/brightness symbol."""
    cx = cy = size / 2 - 0.5
    
    # Sun parameters
    sun_r = 5.0
    ray_inner = sun_r + 3.0
    ray_outer = size / 2 - 8
    num_rays = 8
    ray_width = 0.28  # Radians - width of each ray
    ray_thickness = 2.0  # Pixel width of ray
    
    def brightness_coverage(x: float, y: float) -> float:
        dist = distance(x, y, cx, cy)
        coverage = 0.0
        
        # Sun center circle
        if dist < sun_r + 0.5:
            if dist <= sun_r - 0.5:
                coverage = 1.0
            else:
                coverage = clamp(sun_r + 0.5 - dist)
        
        # Sun rays
        if ray_inner - 0.5 <= dist <= ray_outer + 0.5:
            angle = math.atan2(y - cy, x - cx)
            ray_angle = (2 * math.pi) / num_rays
            
            # Find distance to nearest ray center
            normalized = ((angle % ray_angle) + ray_angle) % ray_angle
            dist_to_ray_center = min(normalized, ray_angle - normalized)
            
            if dist_to_ray_center < ray_width:
                # Ray angular coverage
                angular_factor = clamp(1.0 - dist_to_ray_center / (ray_width * 0.6))
                
                # Ray radial coverage (tapered ends)
                if dist < ray_inner + 1:
                    radial_factor = clamp(dist - ray_inner + 0.5)
                elif dist > ray_outer - 1:
                    radial_factor = clamp(ray_outer + 0.5 - dist)
                else:
                    radial_factor = 1.0
                
                # Ray thickness
                perp_dist = dist * math.sin(dist_to_ray_center)
                thickness_factor = clamp(1.0 - perp_dist / ray_thickness)
                
                ray_coverage = angular_factor * radial_factor * thickness_factor
                coverage = max(coverage, ray_coverage)
        
        return coverage
    
    for y in range(size):
        for x in range(size):
            coverage = sample_pixel_aa(x, y, size, brightness_coverage)
            if coverage > 0.01:
                idx = y * size + x
                pixels[idx] = blend_565(fg_color, pixels[idx], coverage * 0.93)

def draw_wifi_symbol(pixels: List[int], size: int, fg_color: int) -> None:
    """Draw crisp WiFi signal arcs symbol."""
    cx = size / 2 - 0.5
    cy = size / 2 + 4  # Shifted down
    
    # Arc parameters
    radii = [6.5, 11.0, 15.5]  # Arc radii
    arc_width = 2.2
    dot_r = 2.8
    arc_angle = math.pi * 0.75  # 135 degrees total arc
    
    def wifi_coverage(x: float, y: float) -> float:
        dist = distance(x, y, cx, cy)
        coverage = 0.0
        
        # Check angle - only draw upper portion (arcs face up)
        angle = math.atan2(y - cy, x - cx)
        angle_from_up = abs(angle + math.pi / 2)  # Distance from -90 degrees (up)
        
        if angle_from_up < arc_angle / 2:
            for r in radii:
                ring_dist = abs(dist - r)
                if ring_dist < arc_width:
                    # Ring coverage
                    ring_factor = clamp(1.0 - ring_dist / (arc_width * 0.55))
                    
                    # Angular fade at ends
                    angle_edge = arc_angle / 2 - abs(angle_from_up)
                    if angle_edge < 0.2:
                        ring_factor *= clamp(angle_edge / 0.2)
                    
                    coverage = max(coverage, ring_factor)
        
        # Bottom dot
        dot_cy = cy + 1
        dot_dist = distance(x, y, cx, dot_cy)
        if dot_dist < dot_r + 0.5:
            if dot_dist <= dot_r - 0.5:
                coverage = max(coverage, 1.0)
            else:
                coverage = max(coverage, clamp(dot_r + 0.5 - dot_dist))
        
        return coverage
    
    for y in range(size):
        for x in range(size):
            coverage = sample_pixel_aa(x, y, size, wifi_coverage)
            if coverage > 0.01:
                idx = y * size + x
                pixels[idx] = blend_565(fg_color, pixels[idx], coverage * 0.92)

def draw_more_symbol(pixels: List[int], size: int, fg_color: int) -> None:
    """Draw three dots (more/menu) symbol."""
    cx = size / 2 - 0.5
    cy = size / 2 - 0.5
    dot_r = 3.2
    spacing = 10
    
    dot_positions = [
        (cx - spacing, cy),
        (cx, cy),
        (cx + spacing, cy),
    ]
    
    def more_coverage(x: float, y: float) -> float:
        coverage = 0.0
        for dx, dy in dot_positions:
            dist = distance(x, y, dx, dy)
            if dist < dot_r + 0.5:
                if dist <= dot_r - 0.5:
                    coverage = max(coverage, 1.0)
                else:
                    coverage = max(coverage, clamp(dot_r + 0.5 - dist))
        return coverage
    
    for y in range(size):
        for x in range(size):
            coverage = sample_pixel_aa(x, y, size, more_coverage)
            if coverage > 0.01:
                idx = y * size + x
                pixels[idx] = blend_565(fg_color, pixels[idx], coverage * 0.93)

def generate_icon(icon_def: IconDef, transparent: int = 0x0000) -> List[int]:
    """Generate a complete icon with background and symbol."""
    # Create circular background
    pixels = generate_circle_background(ICON_SIZE, icon_def.bg_color, transparent)
    
    # Draw symbol based on type
    symbol_color = 0xFFFF  # White symbols
    
    if icon_def.symbol == 'gear':
        draw_gear_symbol(pixels, ICON_SIZE, symbol_color)
    elif icon_def.symbol == 'target':
        draw_target_symbol(pixels, ICON_SIZE, symbol_color)
    elif icon_def.symbol == 'play':
        draw_play_symbol(pixels, ICON_SIZE, symbol_color)
    elif icon_def.symbol == 'terminal':
        draw_terminal_symbol(pixels, ICON_SIZE, symbol_color)
    elif icon_def.symbol == 'brightness':
        draw_brightness_symbol(pixels, ICON_SIZE, symbol_color)
    elif icon_def.symbol == 'wifi':
        draw_wifi_symbol(pixels, ICON_SIZE, symbol_color)
    elif icon_def.symbol == 'more':
        draw_more_symbol(pixels, ICON_SIZE, symbol_color)
    
    return pixels

def format_pixel_array(name: str, pixels: List[int], width: int) -> str:
    """Format pixel data as C++ array."""
    lines = []
    lines.append(f"static constexpr int kCircularIcon_{name}_W = {ICON_SIZE};")
    lines.append(f"static constexpr int kCircularIcon_{name}_H = {ICON_SIZE};")
    lines.append(f"static constexpr uint16_t kCircularIcon_{name}_Color = 0x0000;")
    lines.append(f"static const uint16_t kCircularIcon_{name}[{len(pixels)}] = {{")
    
    for i in range(0, len(pixels), 16):
        row = pixels[i:i+16]
        hex_values = [f"0x{p:04X}" for p in row]
        line = "  " + ", ".join(hex_values) + ","
        lines.append(line)
    
    lines.append("};")
    return "\n".join(lines)

def main():
    # Define all icons to generate
    icons = [
        IconDef("settings", ICON_COLORS['red'], "gear"),
        IconDef("bounds", ICON_COLORS['blue'], "target"),
        IconDef("live", ICON_COLORS['green'], "play"),
        IconDef("terminal", ICON_COLORS['teal'], "terminal"),
        IconDef("brightness", ICON_COLORS['orange'], "brightness"),
        IconDef("wifi", ICON_COLORS['mint'], "wifi"),
        IconDef("more", ICON_COLORS['gray'], "more"),
    ]
    
    # Generate header file
    print("#pragma once")
    print()
    print("// Auto-generated circular icons for M5Dial launcher UI")
    print("// Style: 42x42 with colored circular backgrounds")
    print("// High-quality with 4x4 multi-sample anti-aliasing")
    print("// Generated by tools/generate_circular_icons.py")
    print()
    print("#include <cstdint>")
    print()
    print("namespace ui::assets {")
    print()
    print("// Icon dimensions")
    print(f"static constexpr int kCircularIconSize = {ICON_SIZE};")
    print(f"static constexpr int kCircularIconRadius = {ICON_RADIUS};")
    print(f"static constexpr uint16_t kCircularIconTransparent = 0x0000;")
    print()
    
    # Print color palette
    print("// Icon background colors (RGB565)")
    print("struct CircularIconColors {")
    for name, color in ICON_COLORS.items():
        print(f"    static constexpr uint16_t {name} = 0x{color:04X};")
    print("};")
    print()
    
    # Generate each icon
    for icon_def in icons:
        pixels = generate_icon(icon_def)
        print(format_pixel_array(icon_def.name, pixels, ICON_SIZE))
        print()
    
    print("} // namespace ui::assets")
    print()

if __name__ == "__main__":
    main()
