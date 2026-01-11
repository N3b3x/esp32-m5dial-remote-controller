/**
 * @file espnow_peer_store.hpp
 * @brief NVS-based storage for approved ESP-NOW peers.
 */

#pragma once

#include <cstdint>

#include "espnow_security.hpp"

namespace PeerStore {

void Init(SecuritySettings& sec,
          const uint8_t* preconfigured_mac = nullptr,
          DeviceType preconfigured_type = DeviceType::Unknown,
          const char* preconfigured_name = nullptr) noexcept;

bool AddPeer(SecuritySettings& sec, const uint8_t mac[6],
             DeviceType type, const char* name) noexcept;

bool RemovePeer(SecuritySettings& sec, const uint8_t mac[6]) noexcept;

bool IsPeerApproved(const SecuritySettings& sec, const uint8_t mac[6]) noexcept;

const ApprovedPeer* GetPeer(const SecuritySettings& sec, const uint8_t mac[6]) noexcept;

bool GetFirstPeerOfType(const SecuritySettings& sec, DeviceType type,
                        uint8_t mac_out[6]) noexcept;

void Save(const SecuritySettings& sec) noexcept;

size_t GetPeerCount(const SecuritySettings& sec) noexcept;

void ClearAll(SecuritySettings& sec) noexcept;

void LogPeers(const SecuritySettings& sec) noexcept;

} // namespace PeerStore
