/**
 * @file settings.hpp
 * @brief Persistent settings (NVS + CRC32).
 * @details Provides settings structures and NVS-based storage with CRC32 validation.
 */

#pragma once

#include <cstdint>

/**
 * @brief Test unit configuration settings
 * @details PROTOCOL V2: Uses direct velocity/acceleration control instead of cycle time.
 */
struct TestUnitSettings {
    // Base settings
    uint32_t cycle_amount = 1000;                     ///< Target cycles (0 = infinite)
    float    oscillation_vmax_rpm = 60.0f;            ///< Max velocity during oscillation (RPM)
    float    oscillation_amax_rev_s2 = 10.0f;         ///< Acceleration during oscillation (rev/s²)
    uint32_t dwell_time_ms = 1000;                    ///< Dwell at endpoints (ms)
    bool     bounds_method_stallguard = true;         ///< true = stallguard, false = encoder

    // Extended settings for bounds finding (0.0f = use test unit defaults)
    float bounds_search_velocity_rpm = 0.0f;          ///< Bounds search velocity (RPM)
    float stallguard_min_velocity_rpm = 0.0f;         ///< Minimum velocity for StallGuard (RPM)
    int8_t stallguard_sgt = 127;                      ///< StallGuard threshold (SGT). Valid range [-64, 63]. 127 = use test unit default
    float stall_detection_current_factor = 0.0f;       ///< Stall detection current factor
    float bounds_search_accel_rev_s2 = 0.0f;         ///< Bounds search acceleration (rev/s²)
};

/**
 * @brief UI display settings
 */
struct UiSettings {
    bool orientation_flipped = false;                 ///< Display orientation flipped
    uint8_t brightness = 128;                         ///< Display brightness (0-255, default 50%)
};

/**
 * @brief Persistent manual bounds data (from manual bounds wizard)
 * @details Stores cached manual bounds so they survive reboots.
 */
struct ManualBoundsSettings {
    bool     valid = false;                           ///< true once wizard has been completed at least once
    float    total_range_deg = 0.0f;                  ///< Full travel (left→right physical stop)
    float    left_backoff_deg = 0.0f;                 ///< Left edge backoff from physical stop
    float    right_backoff_deg = 0.0f;                ///< Right edge backoff from physical stop
};

/**
 * @brief Persistent auto bounds settings
 * @details Stores auto-found bounds (from StallGuard) so they survive reboots,
 *          plus backoff values applied by ESP32.
 */
struct AutoBoundsSettings {
    bool     valid = false;                           ///< true once auto bounds have been found at least once
    float    total_range_deg = 0.0f;                  ///< Full travel found by StallGuard (global_max - global_min)
    float    left_backoff_deg = 3.6f;                 ///< Left backoff from auto-found bound (default 2 full steps)
    float    right_backoff_deg = 3.6f;                ///< Right backoff from auto-found bound (default 2 full steps)
};

/**
 * @brief Complete application settings
 */
struct Settings {
    TestUnitSettings test_unit;                       ///< Test unit configuration
    UiSettings ui;                                   ///< UI display settings
    ManualBoundsSettings manual_bounds;               ///< Cached manual bounds (persisted across reboots)
    AutoBoundsSettings auto_bounds;                   ///< Auto bounds backoff settings
};

/**
 * @brief Settings storage manager using NVS with CRC32 validation
 * @details Provides initialization, loading, and saving of settings with integrity checking.
 */
class SettingsStore {
public:
    /**
     * @brief Initialize NVS flash storage
     * @return true if initialization successful, false otherwise
     */
    static bool Init() noexcept;
    
    /**
     * @brief Load settings from NVS
     * @return Loaded settings, or default settings if load fails or CRC mismatch
     */
    static Settings Load() noexcept;
    
    /**
     * @brief Save settings to NVS with CRC32
     * @param settings Settings to save
     * @return true if save successful, false otherwise
     */
    static bool Save(const Settings& settings) noexcept;
};
