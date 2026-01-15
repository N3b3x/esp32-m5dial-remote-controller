/**
 * @file espnow_peer_store.hpp
 * @brief NVS-based storage for approved ESP-NOW peers.
 */

#pragma once

#include <cstdint>

#include "espnow_security.hpp"

/**
 * @brief NVS-based storage for approved ESP-NOW peers
 * @details Provides peer management with persistent storage and CRC32 validation
 */
namespace PeerStore {

/**
 * @brief Initialize peer store with optional pre-configured peer
 * @param sec Security settings structure to initialize
 * @param preconfigured_mac Optional pre-configured MAC address (nullptr if none)
 * @param preconfigured_type Device type of pre-configured peer
 * @param preconfigured_name Optional name for pre-configured peer
 */
void Init(SecuritySettings& sec,
          const uint8_t* preconfigured_mac = nullptr,
          DeviceType preconfigured_type = DeviceType::Unknown,
          const char* preconfigured_name = nullptr) noexcept;

/**
 * @brief Add a peer to the approved list
 * @param sec Security settings structure
 * @param mac MAC address (6 bytes)
 * @param type Device type
 * @param name Device name (null-terminated, max 16 chars)
 * @return true if added successfully, false if full or invalid
 */
bool AddPeer(SecuritySettings& sec, const uint8_t mac[6],
             DeviceType type, const char* name) noexcept;

/**
 * @brief Remove a peer from the approved list
 * @param sec Security settings structure
 * @param mac MAC address to remove (6 bytes)
 * @return true if removed, false if not found
 */
bool RemovePeer(SecuritySettings& sec, const uint8_t mac[6]) noexcept;

/**
 * @brief Check if a MAC address is an approved peer
 * @param sec Security settings structure
 * @param mac MAC address to check (6 bytes)
 * @return true if approved, false otherwise
 */
bool IsPeerApproved(const SecuritySettings& sec, const uint8_t mac[6]) noexcept;

/**
 * @brief Get peer information by MAC address
 * @param sec Security settings structure
 * @param mac MAC address to lookup (6 bytes)
 * @return Pointer to ApprovedPeer if found, nullptr otherwise
 */
const ApprovedPeer* GetPeer(const SecuritySettings& sec, const uint8_t mac[6]) noexcept;

/**
 * @brief Get first peer of specified device type
 * @param sec Security settings structure
 * @param type Device type to search for
 * @param mac_out Output buffer for MAC address (6 bytes)
 * @return true if peer found, false otherwise
 */
bool GetFirstPeerOfType(const SecuritySettings& sec, DeviceType type,
                        uint8_t mac_out[6]) noexcept;

/**
 * @brief Save peer list to NVS
 * @param sec Security settings structure to save
 */
void Save(const SecuritySettings& sec) noexcept;

/**
 * @brief Get count of approved peers
 * @param sec Security settings structure
 * @return Number of approved peers
 */
size_t GetPeerCount(const SecuritySettings& sec) noexcept;

/**
 * @brief Clear all approved peers
 * @param sec Security settings structure to clear
 */
void ClearAll(SecuritySettings& sec) noexcept;

/**
 * @brief Log all approved peers to ESP log
 * @param sec Security settings structure
 */
void LogPeers(const SecuritySettings& sec) noexcept;

} // namespace PeerStore
