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

static constexpr uint8_t DEVICE_ID_FATIGUE_TESTER_ = 1;

enum class TestState : uint8_t {
    Idle = 0,
    Running,
    Paused,
    Completed,
    Error
};

enum class CommandId : uint8_t {
    Start = 1,
    Pause = 2,
    Resume = 3,
    Stop = 4,

    // NOTE: Not implemented by the current test unit firmware in this repo.
    // Keep this here for forward-compat when/if the unit adds a dedicated
    // bounds-finding command.
    RunBoundsFinding = 5,
};

#pragma pack(push, 1)
struct ConfigPayload {
    // Base fields (13 bytes) - always present
    uint32_t cycle_amount;
    uint32_t time_per_cycle_sec;
    uint32_t dwell_time_sec;
    uint8_t  bounds_method; // 0 = stallguard, 1 = encoder

    // Extended fields (optional, 16 bytes)
    float bounds_search_velocity_rpm;
    float stallguard_min_velocity_rpm;
    float stall_detection_current_factor;
    float bounds_search_accel_rev_s2;
};

struct StatusPayload {
    uint32_t cycle_number;
    uint8_t  state;    // TestState
    uint8_t  err_code;
};

struct BoundsResultPayload {
    uint8_t ok;
    uint8_t bounded;
    uint8_t cancelled;
    uint8_t reserved;
    float   min_degrees_from_center;
    float   max_degrees_from_center;
    float   global_min_degrees;
    float   global_max_degrees;
};
#pragma pack(pop)

static constexpr size_t CONFIG_BASE_SIZE_ = 13;
static constexpr size_t CONFIG_EXTENDED_SIZE_ = sizeof(ConfigPayload);

inline ConfigPayload BuildConfigPayload(const Settings& settings) noexcept
{
    ConfigPayload p{};
    p.cycle_amount = settings.test_unit.cycle_amount;
    p.time_per_cycle_sec = settings.test_unit.time_per_cycle_sec;
    p.dwell_time_sec = settings.test_unit.dwell_time_sec;
    p.bounds_method = settings.test_unit.bounds_method_stallguard ? 0 : 1;

    p.bounds_search_velocity_rpm = settings.test_unit.bounds_search_velocity_rpm;
    p.stallguard_min_velocity_rpm = settings.test_unit.stallguard_min_velocity_rpm;
    p.stall_detection_current_factor = settings.test_unit.stall_detection_current_factor;
    p.bounds_search_accel_rev_s2 = settings.test_unit.bounds_search_accel_rev_s2;
    return p;
}

inline bool ParseStatus(const uint8_t* payload, size_t len, StatusPayload& out) noexcept
{
    if (payload == nullptr || len < sizeof(StatusPayload)) {
        return false;
    }
    std::memcpy(&out, payload, sizeof(StatusPayload));
    return true;
}

inline bool ParseConfig(const uint8_t* payload, size_t len, ConfigPayload& out) noexcept
{
    if (payload == nullptr || len < CONFIG_BASE_SIZE_) {
        return false;
    }

    // Base fields
    std::memcpy(&out.cycle_amount, payload, 4);
    std::memcpy(&out.time_per_cycle_sec, payload + 4, 4);
    std::memcpy(&out.dwell_time_sec, payload + 8, 4);
    out.bounds_method = payload[12];

    // Defaults for extended
    out.bounds_search_velocity_rpm = 0.0f;
    out.stallguard_min_velocity_rpm = 0.0f;
    out.stall_detection_current_factor = 0.0f;
    out.bounds_search_accel_rev_s2 = 0.0f;

    if (len >= CONFIG_EXTENDED_SIZE_) {
        // Copy extended floats at offsets matching ConfigPayload layout.
        std::memcpy(&out.bounds_search_velocity_rpm, payload + 13, 4);
        std::memcpy(&out.stallguard_min_velocity_rpm, payload + 17, 4);
        std::memcpy(&out.stall_detection_current_factor, payload + 21, 4);
        std::memcpy(&out.bounds_search_accel_rev_s2, payload + 25, 4);
    }
    return true;
}

inline bool ParseBoundsResult(const uint8_t* payload, size_t len, BoundsResultPayload& out) noexcept
{
    if (payload == nullptr || len < sizeof(BoundsResultPayload)) {
        return false;
    }
    std::memcpy(&out, payload, sizeof(BoundsResultPayload));
    return true;
}

} // namespace fatigue_proto
