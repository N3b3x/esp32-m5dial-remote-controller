/**
 * @file ui_theme.hpp
 * @brief Modern UI theme and helper functions for M5Dial
 * @details Provides consistent colors, fonts, and drawing helpers for a polished UI.
 *          Leverages M5GFX's anti-aliased drawing, gradients, and arcs.
 */

#pragma once

#include <M5Unified.h>
#include <cstdint>
#include <cmath>

namespace ui {
namespace theme {

// ============================================================================
// Color Palette - Modern dark theme with accent colors
// ============================================================================
/**
 * @brief Color palette namespace
 */
namespace colors {
    // Base colors
    constexpr uint16_t bg_primary     = 0x0000;  ///< Pure black background
    constexpr uint16_t bg_secondary   = 0x18C3;  ///< Dark gray background (#1a1a2e)
    constexpr uint16_t bg_card        = 0x2104;  ///< Card background (#212135)
    constexpr uint16_t bg_elevated    = 0x3186;  ///< Elevated surface background
    
    // Text colors
    constexpr uint16_t text_primary   = 0xFFFF;  ///< Primary text (white)
    constexpr uint16_t text_secondary = 0xB596;  ///< Secondary text (light gray)
    constexpr uint16_t text_muted     = 0x6B6D;  ///< Muted text (gray)
    constexpr uint16_t text_hint      = 0x4228;  ///< Hint text (dark gray)
    
    // Accent colors
    constexpr uint16_t accent_blue    = 0x2D7F;  ///< Vibrant blue accent
    constexpr uint16_t accent_green   = 0x2E89;  ///< Success green accent
    constexpr uint16_t accent_red     = 0xF166;  ///< Error red accent
    constexpr uint16_t accent_yellow  = 0xFE66;  ///< Warning yellow accent
    constexpr uint16_t accent_cyan    = 0x2FFF;  ///< Info cyan accent
    constexpr uint16_t accent_orange  = 0xFC60;  ///< Orange accent
    
    // State colors
    constexpr uint16_t state_idle     = 0x4228;  ///< Idle state (gray)
    constexpr uint16_t state_running  = 0x2E89;  ///< Running state (green)
    constexpr uint16_t state_paused   = 0xFE66;  ///< Paused state (yellow)
    constexpr uint16_t state_error    = 0xF166;  ///< Error state (red)
    constexpr uint16_t state_complete = 0x2FFF;  ///< Complete state (cyan)
    
    // UI element colors
    constexpr uint16_t button_bg      = 0x2104;  ///< Button background
    constexpr uint16_t button_border  = 0x4A69;  ///< Button border
    constexpr uint16_t button_active  = 0x3186;  ///< Active button background
    constexpr uint16_t progress_bg    = 0x2104;  ///< Progress bar background
    constexpr uint16_t selector       = 0xF7BE;  ///< Selector color (cream/off-white)
    
    /**
     * @brief Convert 8-bit RGB to RGB565 format
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @return RGB565 color value
     */
    constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
}

// ============================================================================
// Constants
// ============================================================================
constexpr int16_t DISPLAY_W = 240;   ///< Display width in pixels
constexpr int16_t DISPLAY_H = 240;   ///< Display height in pixels
constexpr int16_t CENTER_X = DISPLAY_W / 2;  ///< Display center X coordinate
constexpr int16_t CENTER_Y = DISPLAY_H / 2;  ///< Display center Y coordinate
constexpr int16_t DISPLAY_R = 120;  ///< Radius of circular display

// ============================================================================
// Drawing Helpers
// ============================================================================

/**
 * @brief Draw an anti-aliased arc gauge (ring segment)
 * @param cx,cy Center position
 * @param r_outer Outer radius
 * @param r_inner Inner radius  
 * @param angle_start Start angle in degrees (0 = right, 90 = down)
 * @param angle_end End angle in degrees
 * @param color Fill color
 */
inline void drawArcGauge(int16_t cx, int16_t cy, int16_t r_outer, int16_t r_inner,
                         float angle_start, float angle_end, uint16_t color) {
    M5.Display.fillArc(cx, cy, r_outer, r_inner, angle_start, angle_end, color);
}

/**
 * @brief Draw progress arc for circular displays
 * @param cx,cy Center position
 * @param radius Radius
 * @param thickness Arc thickness
 * @param progress Progress 0.0-1.0
 * @param fg_color Progress color
 * @param bg_color Background color
 */
inline void drawProgressArc(int16_t cx, int16_t cy, int16_t radius, int16_t thickness,
                            float progress, uint16_t fg_color, uint16_t bg_color) {
    // Background arc (full circle)
    const float start = -90.0f;  // Start at 12 o'clock
    M5.Display.fillArc(cx, cy, radius, radius - thickness, start, start + 360.0f, bg_color);
    
    // Progress arc
    if (progress > 0.001f) {
        const float end = start + 360.0f * std::min(1.0f, progress);
        M5.Display.fillArc(cx, cy, radius, radius - thickness, start, end, fg_color);
    }
}

/**
 * @brief Draw a modern rounded button
 */
inline void drawModernButton(int16_t x, int16_t y, int16_t w, int16_t h,
                             const char* label, bool selected, bool pressed,
                             uint16_t accent_color = colors::accent_blue) {
    uint16_t bg = pressed ? accent_color : (selected ? colors::button_active : colors::button_bg);
    uint16_t border = selected ? accent_color : colors::button_border;
    
    M5.Display.fillSmoothRoundRect(x, y, w, h, h/4, bg);
    M5.Display.drawRoundRect(x, y, w, h, h/4, border);
    
    M5.Display.setTextColor(colors::text_primary);
    M5.Display.setTextSize(1);
    const int16_t tw = M5.Display.textWidth(label);
    M5.Display.setCursor(x + (w - tw) / 2, y + (h - 8) / 2);
    M5.Display.print(label);
}

/**
 * @brief Draw centered text
 */
inline void drawCenteredText(int16_t cx, int16_t y, const char* text, 
                             uint16_t color = colors::text_primary, uint8_t size = 1) {
    M5.Display.setTextSize(size);
    M5.Display.setTextColor(color);
    const int16_t tw = M5.Display.textWidth(text);
    M5.Display.setCursor(cx - tw / 2, y);
    M5.Display.print(text);
}

/**
 * @brief Draw a circular back button (top-left arc)
 */
inline void drawCircularBackButton(bool focused = false) {
    const int16_t r = 35;
    const int16_t cx = 0;
    const int16_t cy = 0;
    
    // Draw arc segment in top-left
    M5.Display.fillArc(cx, cy, r + 10, r - 10, 0, 90, 
                       focused ? colors::accent_blue : colors::bg_elevated);
    
    // Back arrow icon (simple <)
    M5.Display.setTextColor(colors::text_primary);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(12, 12);
    M5.Display.print("<");
}

/**
 * @brief Draw connection status indicator
 */
inline void drawConnectionDot(int16_t x, int16_t y, bool connected, uint32_t now_ms) {
    uint16_t color;
    if (connected) {
        color = colors::accent_green;
    } else {
        // Pulsing animation
        const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now_ms) * 0.006f);
        const uint8_t brightness = static_cast<uint8_t>(128 + 127 * pulse);
        color = colors::rgb565(brightness, brightness / 2, 0);
    }
    
    M5.Display.fillSmoothCircle(x, y, 6, color);
    M5.Display.drawCircle(x, y, 7, colors::text_secondary);
}

/**
 * @brief Apply circular mask effect (darken edges for round display)
 */
inline void drawCircularVignette() {
    // Draw subtle ring at edge to emphasize circular display
    M5.Display.drawCircle(CENTER_X, CENTER_Y, 118, colors::bg_secondary);
    M5.Display.drawCircle(CENTER_X, CENTER_Y, 119, colors::text_hint);
}

/**
 * @brief Draw a value arc with label (for gauges)
 */
inline void drawValueArc(int16_t cx, int16_t cy, int16_t r, 
                         float value, float max_val,
                         const char* label, const char* unit,
                         uint16_t color) {
    // Background arc
    drawProgressArc(cx, cy, r, 12, 0.0f, colors::bg_secondary, colors::progress_bg);
    
    // Value arc
    const float progress = std::min(1.0f, value / max_val);
    drawProgressArc(cx, cy, r, 12, progress, color, colors::progress_bg);
    
    // Center value
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(value));
    drawCenteredText(cx, cy - 20, buf, colors::text_primary, 3);
    
    // Unit
    drawCenteredText(cx, cy + 10, unit, colors::text_muted, 1);
    
    // Label at bottom
    drawCenteredText(cx, cy + r + 15, label, colors::text_secondary, 1);
}

} // namespace theme
} // namespace ui
