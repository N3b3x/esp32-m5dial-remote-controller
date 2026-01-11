/**
 * @file settings.hpp
 * @brief Persistent settings (NVS + CRC32).
 */

#pragma once

#include <cstdint>

/**
 * PROTOCOL V2: Uses direct velocity/acceleration control instead of cycle time.
 */
struct TestUnitSettings {
    // Base settings
    uint32_t cycle_amount = 1000;                     // Target cycles (0 = infinite)
    float    oscillation_vmax_rpm = 60.0f;            // Max velocity during oscillation (RPM)
    float    oscillation_amax_rev_s2 = 10.0f;         // Acceleration during oscillation (rev/s²)
    uint32_t dwell_time_ms = 1000;                    // Dwell at endpoints (ms)
    bool     bounds_method_stallguard = true;         // true = stallguard, false = encoder

    // Extended settings for bounds finding (0.0f = use test unit defaults)
    float bounds_search_velocity_rpm = 0.0f;
    float stallguard_min_velocity_rpm = 0.0f;
    // StallGuard threshold (SGT). Valid range is typically [-64, 63].
    // 127 means "use test unit default" (backward/forward compatible sentinel).
    int8_t stallguard_sgt = 127;
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
