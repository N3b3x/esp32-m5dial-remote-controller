/**
 * @file espnow_security.hpp
 * @brief ESP-NOW security and pairing protocol definitions.
 *
 * Wire-compatible with the fatigue test unit secure pairing protocol.
 * Implements HMAC-SHA256 challenge/response mutual authentication.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "esp_random.h"
#include "mbedtls/md.h"

// ============================================================================
// PAIRING SECRET CONFIGURATION
// ============================================================================
//
// The pairing secret is injected at build time via CMake.
// Configuration sources (in priority order):
//   1. --secret command line argument to build_app.sh
//   2. ESPNOW_PAIRING_SECRET environment variable
//   3. secrets.local.yml file (gitignored)
//   4. Auto-generate for DEBUG builds (with warning)
//   5. Build error for RELEASE builds
//
// See secrets.template.yml for configuration instructions.
// ============================================================================

#ifndef ESPNOW_PAIRING_SECRET_HEX
    #if defined(NDEBUG)
        // RELEASE build without secret - fail with helpful message
        #error "ESPNOW_PAIRING_SECRET not configured for RELEASE build. " \
               "Copy secrets.template.yml to secrets.local.yml and add your secret. " \
               "Generate with: openssl rand -hex 16"
    #else
        // DEBUG build - use placeholder with warning
        #warning "Using auto-generated pairing secret for DEBUG build. NOT SECURE for production!"
        #define ESPNOW_PAIRING_SECRET_HEX "00000000deadbeefcafebabedeadbeef"
    #endif
#endif

static_assert(sizeof(ESPNOW_PAIRING_SECRET_HEX) == 33,
              "ESPNOW_PAIRING_SECRET_HEX must be exactly 32 hex characters");

/**
 * @brief Pairing secret parser for compile-time hex string conversion
 */
namespace PairingSecretParser {
    /**
     * @brief Convert hex character to nibble value
     * @param c Hex character ('0'-'9', 'a'-'f', 'A'-'F')
     * @return Nibble value (0-15)
     */
    constexpr uint8_t HexCharToNibble(char c) noexcept {
        return (c >= '0' && c <= '9') ? static_cast<uint8_t>(c - '0') :
               (c >= 'a' && c <= 'f') ? static_cast<uint8_t>(c - 'a' + 10) :
               (c >= 'A' && c <= 'F') ? static_cast<uint8_t>(c - 'A' + 10) : 0;
    }

    /**
     * @brief Parse hex byte from string at index
     * @param s Hex string (must be at least 2*i+2 chars)
     * @param i Byte index
     * @return Parsed byte value
     */
    constexpr uint8_t HexByte(const char* s, size_t i) noexcept {
        return static_cast<uint8_t>(
            (HexCharToNibble(s[i * 2]) << 4) | HexCharToNibble(s[i * 2 + 1])
        );
    }
} // namespace PairingSecretParser

static constexpr uint8_t PAIRING_SECRET[16] = {
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 0),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 1),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 2),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 3),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 4),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 5),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 6),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 7),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 8),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 9),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 10),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 11),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 12),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 13),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 14),
    PairingSecretParser::HexByte(ESPNOW_PAIRING_SECRET_HEX, 15)
};

static constexpr size_t CHALLENGE_SIZE = 8;                    ///< Challenge size in bytes
static constexpr size_t HMAC_SIZE = 16;                       ///< HMAC output size in bytes
static constexpr size_t MAX_APPROVED_PEERS = 4;                ///< Maximum approved peers
static constexpr size_t MAX_DEVICE_NAME_LEN = 16;            ///< Maximum device name length
static constexpr uint32_t PAIRING_MODE_TIMEOUT_SEC = 30;     ///< Pairing mode timeout (seconds)
static constexpr uint32_t PAIRING_RESPONSE_TIMEOUT_MS = 10000; ///< Pairing response timeout (ms)

static constexpr uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  ///< Broadcast MAC address

/**
 * @brief Device type enumeration
 */
enum class DeviceType : uint8_t {
    Unknown = 0,            ///< Unknown device type
    RemoteController = 1,   ///< Remote controller device
    FatigueTester = 2,      ///< Fatigue test unit device
};

/**
 * @brief Pairing rejection reason codes
 */
enum class PairingRejectReason : uint8_t {
    NotInPairingMode = 0,   ///< Device not in pairing mode
    WrongDeviceType = 1,    ///< Wrong device type
    HmacFailed = 2,         ///< HMAC verification failed
    AlreadyPaired = 3,       ///< Device already paired
    ProtocolMismatch = 4,    ///< Protocol version mismatch
};

#pragma pack(push, 1)
/**
 * @brief Pairing request payload
 */
struct PairingRequestPayload {
    uint8_t requester_mac[6];           ///< Requester MAC address
    uint8_t device_type;                ///< Requester device type
    uint8_t expected_peer_type;         ///< Expected peer device type
    uint8_t challenge[CHALLENGE_SIZE];   ///< Challenge bytes
    uint8_t protocol_version;          ///< Protocol version
};

/**
 * @brief Pairing response payload
 */
struct PairingResponsePayload {
    uint8_t responder_mac[6];          ///< Responder MAC address
    uint8_t device_type;                ///< Responder device type
    uint8_t challenge[CHALLENGE_SIZE];  ///< Responder challenge
    uint8_t hmac_response[HMAC_SIZE];   ///< HMAC response to requester challenge
    char device_name[MAX_DEVICE_NAME_LEN];  ///< Device name (null-terminated)
};

/**
 * @brief Pairing confirmation payload
 */
struct PairingConfirmPayload {
    uint8_t confirmer_mac[6];          ///< Confirmer MAC address
    uint8_t hmac_response[HMAC_SIZE];   ///< HMAC response to responder challenge
    uint8_t success;                    ///< 1 if successful, 0 otherwise
};

/**
 * @brief Pairing rejection payload
 */
struct PairingRejectPayload {
    uint8_t rejecter_mac[6];            ///< Rejecter MAC address
    uint8_t reason;                      ///< Rejection reason (PairingRejectReason)
};
#pragma pack(pop)

/**
 * @brief Approved peer information
 */
struct ApprovedPeer {
    uint8_t mac[6];                     ///< Peer MAC address
    uint8_t device_type;                 ///< Device type
    char name[MAX_DEVICE_NAME_LEN];      ///< Device name (null-terminated)
    uint32_t paired_timestamp;           ///< Pairing timestamp (unused, reserved)
    bool valid;                          ///< True if peer entry is valid
};

/**
 * @brief Security settings structure
 */
struct SecuritySettings {
    ApprovedPeer approved_peers[MAX_APPROVED_PEERS];  ///< Array of approved peers
};

/**
 * @brief Compute HMAC-SHA256 for pairing challenge
 * @param challenge Challenge bytes
 * @param challenge_len Challenge length
 * @param out Output buffer for HMAC (must be at least HMAC_SIZE bytes)
 */
inline void ComputePairingHmac(const uint8_t* challenge, size_t challenge_len,
                               uint8_t out[HMAC_SIZE]) noexcept
{
    uint8_t full_hmac[32];

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, PAIRING_SECRET, sizeof(PAIRING_SECRET));
    mbedtls_md_hmac_update(&ctx, challenge, challenge_len);
    mbedtls_md_hmac_finish(&ctx, full_hmac);
    mbedtls_md_free(&ctx);

    std::memcpy(out, full_hmac, HMAC_SIZE);
}

/**
 * @brief Verify HMAC for pairing challenge
 * @param challenge Challenge bytes
 * @param challenge_len Challenge length
 * @param received_hmac Received HMAC to verify
 * @return true if HMAC matches, false otherwise
 */
inline bool VerifyPairingHmac(const uint8_t* challenge, size_t challenge_len,
                              const uint8_t received_hmac[HMAC_SIZE]) noexcept
{
    uint8_t expected[HMAC_SIZE];
    ComputePairingHmac(challenge, challenge_len, expected);

    uint8_t diff = 0;
    for (size_t i = 0; i < HMAC_SIZE; ++i) {
        diff |= (expected[i] ^ received_hmac[i]);
    }
    return diff == 0;
}

/**
 * @brief Generate random challenge for pairing
 * @param out Output buffer for challenge (must be at least CHALLENGE_SIZE bytes)
 */
inline void GenerateChallenge(uint8_t out[CHALLENGE_SIZE]) noexcept
{
    for (size_t i = 0; i < CHALLENGE_SIZE; ++i) {
        out[i] = static_cast<uint8_t>(esp_random() & 0xFF);
    }
}

/**
 * @brief Check if MAC address is all zeros
 * @param mac MAC address (6 bytes)
 * @return true if all zeros, false otherwise
 */
inline bool IsZeroMac(const uint8_t mac[6]) noexcept
{
    return mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0 && mac[4] == 0 && mac[5] == 0;
}

/**
 * @brief Compare two MAC addresses for equality
 * @param a First MAC address (6 bytes)
 * @param b Second MAC address (6 bytes)
 * @return true if equal, false otherwise
 */
inline bool MacEquals(const uint8_t a[6], const uint8_t b[6]) noexcept
{
    return std::memcmp(a, b, 6) == 0;
}
