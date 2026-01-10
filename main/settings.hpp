/**
 * @file settings.hpp
 * @brief Persistent settings (NVS + CRC32).
 */

#pragma once

#include <cstdint>

struct TestUnitSettings {
    uint32_t cycle_amount = 1000;
    uint32_t time_per_cycle_sec = 5;
    uint32_t dwell_time_sec = 1;
    bool bounds_method_stallguard = true;

    float bounds_search_velocity_rpm = 0.0f;
    float stallguard_min_velocity_rpm = 0.0f;
    float stall_detection_current_factor = 0.0f;
    float bounds_search_accel_rev_s2 = 0.0f;
};

struct UiSettings {
    bool orientation_flipped = false;
    uint8_t brightness = 128;  // 0-255, default 50%
};

struct Settings {
    TestUnitSettings test_unit;
    UiSettings ui;
};

class SettingsStore {
public:
    static bool Init() noexcept;
    static Settings Load() noexcept;
    static bool Save(const Settings& settings) noexcept;
};
