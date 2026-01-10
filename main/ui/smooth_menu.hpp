/**
 * @file smooth_menu.hpp
 * @brief Smooth animated menu system for M5Dial circular UI
 * @details Implements overshoot easing animation for selector movement,
 *          matching the M5Dial factory demo UI style.
 *
 * Key features:
 * - Circular icon positioning with cos/sin
 * - White selector dot with overshoot animation
 * - Smooth transitions between menu items
 * - Touch support for center circle press
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace ui {

/**
 * @brief LVGL-style animation easing functions
 */
namespace ease {

/**
 * @brief Overshoot easing - goes beyond target then bounces back
 * @param t Progress (0.0 to 1.0)
 * @return Eased value (may exceed 1.0 temporarily)
 */
inline float overshoot(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

/**
 * @brief Ease-out cubic
 */
inline float easeOutCubic(float t) {
    return 1.0f - std::pow(1.0f - t, 3.0f);
}

/**
 * @brief Linear interpolation
 */
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace ease

/**
 * @brief Animated position value with easing
 */
class AnimatedValue {
public:
    AnimatedValue() = default;

    void setTarget(float target, uint32_t duration_ms, uint32_t current_time_ms) {
        start_value_ = getCurrentValue(current_time_ms);
        target_value_ = target;
        anim_start_ms_ = current_time_ms;
        anim_duration_ms_ = duration_ms;
    }

    void setImmediate(float value, uint32_t current_time_ms) {
        start_value_ = value;
        target_value_ = value;
        anim_start_ms_ = current_time_ms;
        anim_duration_ms_ = 0;
    }

    float getCurrentValue(uint32_t current_time_ms) const {
        if (anim_duration_ms_ == 0) {
            return target_value_;
        }
        const uint32_t elapsed = current_time_ms - anim_start_ms_;
        if (elapsed >= anim_duration_ms_) {
            return target_value_;
        }
        const float t = static_cast<float>(elapsed) / static_cast<float>(anim_duration_ms_);
        const float eased_t = ease::overshoot(t);
        return ease::lerp(start_value_, target_value_, eased_t);
    }

    bool isAnimating(uint32_t current_time_ms) const {
        if (anim_duration_ms_ == 0) {
            return false;
        }
        return (current_time_ms - anim_start_ms_) < anim_duration_ms_;
    }

    float getTarget() const { return target_value_; }

private:
    float start_value_ = 0.0f;
    float target_value_ = 0.0f;
    uint32_t anim_start_ms_ = 0;
    uint32_t anim_duration_ms_ = 0;
};

/**
 * @brief Point in 2D space
 */
struct Point2D {
    float x = 0.0f;
    float y = 0.0f;
};

/**
 * @brief Menu item configuration
 */
struct MenuItem {
    const char* tag_up = nullptr;      // First line of label (shown in center)
    const char* tag_down = nullptr;    // Second line of label (optional)
    uint16_t color = 0xFFFF;           // Icon background color (RGB565)
    const uint16_t* icon_data = nullptr;  // Pointer to icon pixel data (42x42 RGB565)
    int16_t icon_w = 42;
    int16_t icon_h = 42;
};

/**
 * @brief Configuration for the circular menu
 */
struct CircularMenuConfig {
    // Center of the display
    int16_t center_x = 120;
    int16_t center_y = 120;
    
    // Radii for positioning
    int16_t icon_radius = 95;          // Distance from center to icon centers
    int16_t selector_radius = 60;      // Distance from center to selector dot
    
    // Icon appearance
    int16_t icon_bg_radius = 22;       // Radius of circular icon background
    int16_t icon_selected_offset = 3;  // Extra radius for selected icon
    float icon_selected_scale = 1.1f;  // Scale factor for selected icon
    
    // Selector appearance
    int16_t selector_dot_radius = 5;   // Radius of the white selector dot
    uint16_t selector_color = 0xF3E9;  // 0xF3E9D2 converted to RGB565 (cream/off-white)
    
    // Animation
    uint32_t anim_duration_ms = 300;   // Duration for selector animation
    
    // Interaction
    int16_t center_touch_radius = 50;  // Touch radius for center button
    
    // Theme colors
    uint16_t theme_fg = 0xFA00;        // 0xFA7000 -> orange theme
    uint16_t theme_bg = 0x0000;        // Black background
};

/**
 * @brief Circular menu selector with smooth animation
 */
class CircularMenuSelector {
public:
    CircularMenuSelector() = default;

    void init(const CircularMenuConfig& config, int num_items) {
        config_ = config;
        num_items_ = num_items;
        computePositions_();
    }

    void setSelectedIndex(int index, uint32_t current_time_ms, bool animate = true) {
        if (index < 0 || index >= num_items_) {
            return;
        }
        
        if (animate && index != selected_index_) {
            // Animate selector to new position
            const Point2D& target = selector_positions_[index];
            selector_x_.setTarget(target.x, config_.anim_duration_ms, current_time_ms);
            selector_y_.setTarget(target.y, config_.anim_duration_ms, current_time_ms);
        } else {
            // Immediate jump
            const Point2D& target = selector_positions_[index];
            selector_x_.setImmediate(target.x, current_time_ms);
            selector_y_.setImmediate(target.y, current_time_ms);
        }
        
        selected_index_ = index;
    }

    void goNext(uint32_t current_time_ms) {
        int next = (selected_index_ + 1) % num_items_;
        setSelectedIndex(next, current_time_ms, true);
    }

    void goPrev(uint32_t current_time_ms) {
        int prev = (selected_index_ - 1 + num_items_) % num_items_;
        setSelectedIndex(prev, current_time_ms, true);
    }

    int getSelectedIndex() const { return selected_index_; }

    Point2D getSelectorPosition(uint32_t current_time_ms) const {
        return {
            selector_x_.getCurrentValue(current_time_ms),
            selector_y_.getCurrentValue(current_time_ms)
        };
    }

    Point2D getIconPosition(int index) const {
        if (index < 0 || index >= num_items_ || index >= kMaxItems) {
            return {0, 0};
        }
        return icon_positions_[index];
    }

    bool isAnimating(uint32_t current_time_ms) const {
        return selector_x_.isAnimating(current_time_ms) || selector_y_.isAnimating(current_time_ms);
    }

    const CircularMenuConfig& getConfig() const { return config_; }
    int getNumItems() const { return num_items_; }

private:
    static constexpr int kMaxItems = 12;  // Maximum supported menu items
    static constexpr float kPi = 3.14159265f;

    void computePositions_() {
        // Pre-compute icon and selector positions in a circle
        // Start from -90 degrees (12 o'clock position) and go clockwise
        const float start_angle = -kPi / 2.0f;
        const float angle_step = (2.0f * kPi) / static_cast<float>(num_items_);
        
        for (int i = 0; i < num_items_ && i < kMaxItems; ++i) {
            const float angle = start_angle + static_cast<float>(i) * angle_step;
            
            // Icon positions (outer ring)
            icon_positions_[i].x = config_.center_x + config_.icon_radius * std::cos(angle);
            icon_positions_[i].y = config_.center_y + config_.icon_radius * std::sin(angle);
            
            // Selector positions (inner ring)
            selector_positions_[i].x = config_.center_x + config_.selector_radius * std::cos(angle);
            selector_positions_[i].y = config_.center_y + config_.selector_radius * std::sin(angle);
        }
    }

    CircularMenuConfig config_;
    int num_items_ = 0;
    int selected_index_ = 0;
    
    Point2D icon_positions_[kMaxItems];
    Point2D selector_positions_[kMaxItems];
    
    AnimatedValue selector_x_;
    AnimatedValue selector_y_;
};

} // namespace ui
