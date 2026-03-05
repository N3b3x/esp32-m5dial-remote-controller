/**
 * @file fatigue_protocol.hpp
 * @brief Fatigue test unit payload definitions for ESP-NOW messages.
 *
 * These structures are intentionally kept wire-compatible with the existing
 * fatigue test unit (examples/esp32/main/fatigue_test_espnow).
 */

#pragma once

#include <cstdint>
#include <cstring>

#include "../settings.hpp"

namespace fatigue_proto {

static constexpr uint8_t DEVICE_ID_FATIGUE_TESTER_ = 1;  ///< Device ID for fatigue tester

/**
 * @brief Test execution state
 */
enum class TestState : uint8_t {
    Idle = 0,       ///< Test idle/stopped
    Running,        ///< Test running
    Paused,        ///< Test paused
    Completed,      ///< Test completed
    Error           ///< Test error state
};

/**
 * @brief Command identifiers for fatigue test control
 */
enum class CommandId : uint8_t {
    Start = 1,              ///< Start test
    Pause = 2,              ///< Pause test
    Resume = 3,             ///< Resume paused test
    Stop = 4,               ///< Stop test
    RunBoundsFinding = 5,   ///< Run bounds finding (dedicated command)
    SetManualBounds = 6,    ///< (legacy, unused) Set manual bounds single-shot
    ManualBoundsStart = 7,      ///< Start manual bounds mode: disengage motor
    ManualBoundsArmPlaced = 8,  ///< Arm at left stop: engage motor, set reference
    ManualBoundsJog = 9,        ///< Jog to abs position (float: degrees from left stop)
    ManualBoundsConfirm = 10,   ///< Confirm bounds (payload: center_deg f32, local_gap_deg f32)
    ManualBoundsCancel = 11,    ///< Cancel manual bounds finding
    ManualBoundsReZero = 12,    ///< Re-zero motor position at current location
    StartWithManualBounds = 13, ///< Start test reusing cached manual bounds (center, gap, fix_offset)
    StartWithManualRealign = 14, ///< Start test after manual encoder realignment (skip hard-drive)
};

/**
 * @brief Configuration payload for fatigue test
 * @details PROTOCOL V2: Uses direct velocity/acceleration control instead of cycle time.
 */
#pragma pack(push, 1)
struct ConfigPayload {
    // Base fields (17 bytes) - always present
    uint32_t cycle_amount;                     ///< Target cycles (0 = infinite)
    float    oscillation_vmax_rpm;             ///< Max oscillation velocity (RPM) - direct to TMC5160 VMAX
    float    oscillation_amax_rev_s2;          ///< Oscillation acceleration (rev/s²) - direct to TMC5160 AMAX
    uint32_t dwell_time_ms;                    ///< Dwell time at endpoints (milliseconds)
    uint8_t  bounds_method;                    ///< 0 = stallguard, 1 = encoder

    // Extended fields (optional, 16 bytes) - for bounds finding configuration
    float bounds_search_velocity_rpm;          ///< Bounds search velocity (RPM)
    float stallguard_min_velocity_rpm;         ///< Minimum velocity for StallGuard (RPM)
    float stall_detection_current_factor;      ///< Stall detection current factor
    float bounds_search_accel_rev_s2;         ///< Bounds search acceleration (rev/s²)

    // Extended v2 field (optional, 1 byte)
    int8_t stallguard_sgt;                     ///< StallGuard threshold (SGT). Valid range [-64, 63]. 127 = use test unit default

    // Extended v3 fields (optional, 26 bytes) — persistent bounds from unit NVS
    uint8_t manual_bounds_valid;               ///< 1 if manual bounds stored on unit
    float   manual_total_range_deg;            ///< Manual bounds total travel (degrees)
    float   manual_left_backoff_deg;           ///< Manual left backoff (degrees)
    float   manual_right_backoff_deg;          ///< Manual right backoff (degrees)
    uint8_t auto_bounds_valid;                 ///< 1 if auto bounds stored on unit
    float   auto_total_range_deg;              ///< Auto bounds total travel (degrees)
    float   auto_left_backoff_deg;             ///< Auto left backoff (degrees)
    float   auto_right_backoff_deg;            ///< Auto right backoff (degrees)
};

/**
 * @brief Status payload from fatigue test unit
 */
struct StatusPayload {
    uint32_t cycle_number;                    ///< Current cycle number
    uint8_t  state;                           ///< Test state (TestState)
    uint8_t  err_code;                        ///< Error code (0 = no error)
    uint8_t  bounds_valid;                    ///< 1 = bounds reusable, 0 = invalid, 255 = unknown
};

/**
 * @brief Bounds finding result payload
 */
struct BoundsResultPayload {
    uint8_t ok;                              ///< 1 if bounds finding succeeded, 0 otherwise
    uint8_t bounded;                         ///< 1 if bounds were found, 0 otherwise
    uint8_t cancelled;                      ///< 1 if cancelled, 0 otherwise
    uint8_t reserved;                        ///< Reserved field
    float   min_degrees_from_center;          ///< Minimum angle from center (degrees)
    float   max_degrees_from_center;         ///< Maximum angle from center (degrees)
    float   global_min_degrees;              ///< Global minimum angle (degrees)
    float   global_max_degrees;              ///< Global maximum angle (degrees)
};
#pragma pack(pop)

static constexpr size_t CONFIG_BASE_SIZE_ = 17;              ///< Base fields size (17 bytes)
static constexpr size_t CONFIG_EXTENDED_V1_SIZE_ = 33;      ///< Extended v1 size (33 bytes)
static constexpr size_t CONFIG_EXTENDED_V2_SIZE_ = 34;      ///< Extended v2 size (34 bytes, + SGT)
static constexpr size_t CONFIG_EXTENDED_V3_SIZE_ = sizeof(ConfigPayload); ///< Extended v3 size (v2 + bounds)
static constexpr size_t CONFIG_EXTENDED_SIZE_ = sizeof(ConfigPayload);  ///< Full extended size

/**
 * @brief Build configuration payload from settings
 * @param settings Application settings
 * @return Configuration payload
 */
inline ConfigPayload BuildConfigPayload(const Settings& settings) noexcept
{
    ConfigPayload p{};
    p.cycle_amount = settings.test_unit.cycle_amount;
    p.oscillation_vmax_rpm = settings.test_unit.oscillation_vmax_rpm;
    p.oscillation_amax_rev_s2 = settings.test_unit.oscillation_amax_rev_s2;
    p.dwell_time_ms = settings.test_unit.dwell_time_ms;
    p.bounds_method = settings.test_unit.bounds_method_stallguard ? 0 : 1;

    p.bounds_search_velocity_rpm = settings.test_unit.bounds_search_velocity_rpm;
    p.stallguard_min_velocity_rpm = settings.test_unit.stallguard_min_velocity_rpm;
    p.stall_detection_current_factor = settings.test_unit.stall_detection_current_factor;
    p.bounds_search_accel_rev_s2 = settings.test_unit.bounds_search_accel_rev_s2;
    p.stallguard_sgt = settings.test_unit.stallguard_sgt;

    // v3: include cached bounds so ConfigSet syncs edits to unit
    p.manual_bounds_valid = settings.manual_bounds.valid ? 1 : 0;
    p.manual_total_range_deg = settings.manual_bounds.total_range_deg;
    p.manual_left_backoff_deg = settings.manual_bounds.left_backoff_deg;
    p.manual_right_backoff_deg = settings.manual_bounds.right_backoff_deg;
    p.auto_bounds_valid = settings.auto_bounds.valid ? 1 : 0;
    p.auto_total_range_deg = settings.auto_bounds.total_range_deg;
    p.auto_left_backoff_deg = settings.auto_bounds.left_backoff_deg;
    p.auto_right_backoff_deg = settings.auto_bounds.right_backoff_deg;
    return p;
}

/**
 * @brief Parse status payload from received data
 * @param payload Payload data buffer
 * @param len Payload length
 * @param out Output status payload structure
 * @return true if parsing successful, false otherwise
 */
inline bool ParseStatus(const uint8_t* payload, size_t len, StatusPayload& out) noexcept
{
    // Backward compatible: older units send only the first 6 bytes.
    if (payload == nullptr || len < 6) {
        return false;
    }

    std::memcpy(&out.cycle_number, payload, 4);
    out.state = payload[4];
    out.err_code = payload[5];
    out.bounds_valid = (len >= 7) ? payload[6] : 255;
    return true;
}

/**
 * @brief Parse configuration payload from received data
 * @details Supports backward compatibility with base, extended v1, and extended v2 formats
 * @param payload Payload data buffer
 * @param len Payload length
 * @param out Output configuration payload structure
 * @return true if parsing successful, false otherwise
 */
inline bool ParseConfig(const uint8_t* payload, size_t len, ConfigPayload& out) noexcept
{
    if (payload == nullptr || len < CONFIG_BASE_SIZE_) {
        return false;
    }

    // Base fields (17 bytes): cycle_amount(4) + vmax(4) + amax(4) + dwell_ms(4) + bounds_method(1)
    std::memcpy(&out.cycle_amount, payload, 4);
    std::memcpy(&out.oscillation_vmax_rpm, payload + 4, 4);
    std::memcpy(&out.oscillation_amax_rev_s2, payload + 8, 4);
    std::memcpy(&out.dwell_time_ms, payload + 12, 4);
    out.bounds_method = payload[16];

    // Defaults for extended bounds finding config
    out.bounds_search_velocity_rpm = 0.0f;
    out.stallguard_min_velocity_rpm = 0.0f;
    out.stall_detection_current_factor = 0.0f;
    out.bounds_search_accel_rev_s2 = 0.0f;
    out.stallguard_sgt = 127;

    if (len >= CONFIG_EXTENDED_V1_SIZE_) {
        // Copy extended floats at offsets matching ConfigPayload layout.
        std::memcpy(&out.bounds_search_velocity_rpm, payload + 17, 4);
        std::memcpy(&out.stallguard_min_velocity_rpm, payload + 21, 4);
        std::memcpy(&out.stall_detection_current_factor, payload + 25, 4);
        std::memcpy(&out.bounds_search_accel_rev_s2, payload + 29, 4);
    }

    if (len >= CONFIG_EXTENDED_V2_SIZE_) {
        std::memcpy(&out.stallguard_sgt, payload + 33, 1);
    }

    // v3 defaults
    out.manual_bounds_valid = 0;
    out.manual_total_range_deg = 0.0f;
    out.manual_left_backoff_deg = 0.0f;
    out.manual_right_backoff_deg = 0.0f;
    out.auto_bounds_valid = 0;
    out.auto_total_range_deg = 0.0f;
    out.auto_left_backoff_deg = 0.0f;
    out.auto_right_backoff_deg = 0.0f;

    if (len >= CONFIG_EXTENDED_V3_SIZE_) {
        // v3: offset 34 = manual_bounds_valid(1) + total(4) + left(4) + right(4)
        //     offset 47 = auto_bounds_valid(1) + total(4) + left(4) + right(4)
        out.manual_bounds_valid = payload[34];
        std::memcpy(&out.manual_total_range_deg, payload + 35, 4);
        std::memcpy(&out.manual_left_backoff_deg, payload + 39, 4);
        std::memcpy(&out.manual_right_backoff_deg, payload + 43, 4);
        out.auto_bounds_valid = payload[47];
        std::memcpy(&out.auto_total_range_deg, payload + 48, 4);
        std::memcpy(&out.auto_left_backoff_deg, payload + 52, 4);
        std::memcpy(&out.auto_right_backoff_deg, payload + 56, 4);
    }
    return true;
}

/**
 * @brief Parse bounds result payload from received data
 * @param payload Payload data buffer
 * @param len Payload length
 * @param out Output bounds result payload structure
 * @return true if parsing successful, false otherwise
 */
inline bool ParseBoundsResult(const uint8_t* payload, size_t len, BoundsResultPayload& out) noexcept
{
    if (payload == nullptr || len < sizeof(BoundsResultPayload)) {
        return false;
    }
    std::memcpy(&out, payload, sizeof(BoundsResultPayload));
    return true;
}

} // namespace fatigue_proto
