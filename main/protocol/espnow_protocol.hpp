/**
 * @file espnow_protocol.hpp
 * @brief Generic ESP-NOW protocol for M5Dial remote controller.
 */

#pragma once

#include <cstdint>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"

#include "espnow_security.hpp"

namespace espnow {

static constexpr uint8_t SYNC_BYTE_ = 0xAA;           ///< Packet sync byte
static constexpr uint8_t PROTOCOL_VERSION_ = 1;        ///< Protocol version number
static constexpr uint8_t MAX_PAYLOAD_SIZE_ = 200;     ///< Maximum payload size in bytes
static constexpr uint16_t CRC16_POLYNOMIAL_ = 0x1021; ///< CRC16 polynomial (CCITT)
static constexpr uint8_t WIFI_CHANNEL_ = 1;            ///< WiFi channel for ESP-NOW

/**
 * @brief ESP-NOW message types
 */
enum class MsgType : uint8_t {
    DeviceDiscovery = 1,   ///< Device discovery broadcast
    DeviceInfo,            ///< Device information response
    ConfigRequest,         ///< Request configuration from device
    ConfigResponse,        ///< Configuration response
    ConfigSet,             ///< Set configuration on device
    ConfigAck,             ///< Configuration acknowledge
    Command,               ///< Send command to device
    CommandAck,            ///< Command acknowledge
    StatusUpdate,          ///< Status update from device
    Error,                 ///< Error message
    ErrorClear,            ///< Clear error
    TestComplete,          ///< Test completion notification

    // Fatigue-test extensions
    BoundsResult = 13,     ///< Bounds finding result

    // Security / Pairing messages (20-29 range)
    PairingRequest = 20,    ///< Pairing request
    PairingResponse = 21,   ///< Pairing response
    PairingConfirm = 22,    ///< Pairing confirmation
    PairingReject = 23,     ///< Pairing rejection
    Unpair = 24,           ///< Unpair device
};

/**
 * @brief Pairing state machine states
 */
enum class PairingState : uint8_t {
    Idle,                  ///< Not pairing
    WaitingForResponse,    ///< Waiting for pairing response
    Complete,              ///< Pairing completed successfully
    Failed,                ///< Pairing failed
};

#pragma pack(push, 1)
/**
 * @brief ESP-NOW packet header
 */
struct EspNowHeader {
    uint8_t sync;      ///< Sync byte (SYNC_BYTE_)
    uint8_t version;   ///< Protocol version
    uint8_t device_id; ///< Device identifier
    uint8_t type;      ///< Message type (MsgType)
    uint8_t id;        ///< Message sequence ID
    uint8_t len;       ///< Payload length
};

/**
 * @brief Complete ESP-NOW packet structure
 */
struct EspNowPacket {
    EspNowHeader hdr;                      ///< Packet header
    uint8_t payload[MAX_PAYLOAD_SIZE_];   ///< Payload data
    uint16_t crc;                         ///< CRC16 checksum
};
#pragma pack(pop)

/**
 * @brief Protocol event for application layer
 */
struct ProtoEvent {
    MsgType type;                          ///< Message type
    uint8_t device_id;                     ///< Source device ID
    uint8_t sequence_id;                   ///< Message sequence ID
    uint8_t payload[MAX_PAYLOAD_SIZE_];    ///< Event payload
    size_t payload_len;                    ///< Payload length
    uint8_t src_mac[6];                    ///< Source MAC address
};

/**
 * @brief Initialize ESP-NOW protocol stack
 * @param event_queue FreeRTOS queue for protocol events
 * @return true if initialization successful, false otherwise
 */
bool Init(QueueHandle_t event_queue) noexcept;

/**
 * @brief Send device discovery broadcast
 * @return true if send successful, false otherwise
 */
bool SendDeviceDiscovery() noexcept;

/**
 * @brief Send configuration request to device
 * @param device_id Target device ID
 * @return true if send successful, false otherwise
 */
bool SendConfigRequest(uint8_t device_id) noexcept;

/**
 * @brief Send configuration set command to device
 * @param device_id Target device ID
 * @param config_data Configuration data buffer
 * @param config_len Configuration data length
 * @return true if send successful, false otherwise
 */
bool SendConfigSet(uint8_t device_id, const void* config_data, size_t config_len) noexcept;

/**
 * @brief Send command to device
 * @param device_id Target device ID
 * @param command_id Command identifier
 * @param payload Command payload (may be nullptr)
 * @param payload_len Payload length
 * @return true if send successful, false otherwise
 */
bool SendCommand(uint8_t device_id, uint8_t command_id, const void* payload, size_t payload_len) noexcept;

// ============================================================================
// PAIRING / PEER MANAGEMENT
// ============================================================================

/**
 * @brief Start pairing process
 * @return true if pairing started, false if already in progress
 */
bool StartPairing() noexcept;

/**
 * @brief Cancel ongoing pairing process
 */
void CancelPairing() noexcept;

/**
 * @brief Get current pairing state
 * @return Current pairing state
 */
PairingState GetPairingState() noexcept;

/**
 * @brief Get security settings reference
 * @return Reference to security settings
 */
SecuritySettings& GetSecuritySettings() noexcept;

/**
 * @brief Check if MAC address is an approved peer
 * @param mac MAC address to check (6 bytes)
 * @return true if approved, false otherwise
 */
bool IsPeerApproved(const uint8_t mac[6]) noexcept;

/**
 * @brief Add an approved peer
 * @param mac MAC address (6 bytes)
 * @param type Device type
 * @param name Device name (null-terminated, max 16 chars)
 * @return true if added successfully, false if full or invalid
 */
bool AddApprovedPeer(const uint8_t mac[6], DeviceType type, const char* name) noexcept;

/**
 * @brief Remove an approved peer
 * @param mac MAC address to remove (6 bytes)
 * @return true if removed, false if not found
 */
bool RemoveApprovedPeer(const uint8_t mac[6]) noexcept;

/**
 * @brief Get count of approved peers
 * @return Number of approved peers
 */
size_t GetApprovedPeerCount() noexcept;

/**
 * @brief Get MAC address of target device (first FatigueTester peer)
 * @param mac_out Output buffer for MAC address (6 bytes)
 * @return true if target found, false otherwise
 */
bool GetTargetDeviceMac(uint8_t mac_out[6]) noexcept;

/**
 * @brief Compute CRC16-CCITT checksum
 * @param data Data buffer
 * @param len Data length in bytes
 * @return CRC16 checksum value
 */
inline uint16_t crc16_ccitt(const uint8_t* data, size_t len) noexcept
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_POLYNOMIAL_;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

} // namespace espnow
