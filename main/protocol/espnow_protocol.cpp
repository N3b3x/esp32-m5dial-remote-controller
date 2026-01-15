/**
 * @file espnow_protocol.cpp
 * @brief ESP-NOW protocol implementation for M5Dial remote controller with secure pairing.
 */

#include "espnow_protocol.hpp"

#include "espnow_peer_store.hpp"

#include "../config.hpp"

#include <cstring>

#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char* TAG_ = "espnow";

static QueueHandle_t s_proto_event_queue_ = nullptr;
static uint8_t s_next_msg_id_ = 1;
static QueueHandle_t s_raw_recv_queue_ = nullptr;

static SecuritySettings s_security_{};

static espnow::PairingState s_pairing_state_ = espnow::PairingState::Idle;
static uint8_t s_my_challenge_[CHALLENGE_SIZE] = {0};
static TickType_t s_pairing_timeout_tick_ = 0;

struct RawMsg {
    uint8_t data[sizeof(espnow::EspNowPacket)];
    int len;
    uint8_t src_mac[6];
};

// Forward declarations
static void espnowRecvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len);
static void espnowSendCb(const wifi_tx_info_t* info, esp_now_send_status_t status);
static void recvTask(void*);
static void handlePacket(const RawMsg& msg, const uint8_t* data, int len);
static void handlePairingResponse(const uint8_t* src_mac, const espnow::EspNowPacket& pkt);
static void handlePairingReject(const uint8_t* src_mac, const espnow::EspNowPacket& pkt);

/**
 * @brief Try to add ESP-NOW peer (ignores if already exists)
 * @param mac MAC address (6 bytes)
 */
static void tryAddEspNowPeer(const uint8_t mac[6])
{
    if (IsZeroMac(mac)) return;

    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, mac, 6);
    peer.channel = espnow::WIFI_CHANNEL_;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    const esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGW(TAG_, "Failed to add peer: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Send ESP-NOW packet to specific MAC address
 * @param dst_mac Destination MAC address (6 bytes)
 * @param device_id Device identifier
 * @param type Message type
 * @param payload Payload data (may be nullptr)
 * @param payload_len Payload length
 * @return true if send successful, false otherwise
 */
static bool sendPacketTo(const uint8_t* dst_mac, uint8_t device_id,
                         espnow::MsgType type, const void* payload, uint8_t payload_len)
{
    if (payload_len > espnow::MAX_PAYLOAD_SIZE_) {
        ESP_LOGE(TAG_, "Payload too big: %u", payload_len);
        return false;
    }

    uint8_t send_buf[sizeof(espnow::EspNowHeader) + espnow::MAX_PAYLOAD_SIZE_ + sizeof(uint16_t)];

    auto* hdr = reinterpret_cast<espnow::EspNowHeader*>(send_buf);
    hdr->sync = espnow::SYNC_BYTE_;
    hdr->version = espnow::PROTOCOL_VERSION_;
    hdr->device_id = device_id;
    hdr->type = static_cast<uint8_t>(type);
    hdr->id = s_next_msg_id_++;
    hdr->len = payload_len;

    if (payload_len > 0 && payload != nullptr) {
        std::memcpy(send_buf + sizeof(espnow::EspNowHeader), payload, payload_len);
    }

    const size_t crc_data_len = sizeof(espnow::EspNowHeader) + payload_len;
    const uint16_t crc = espnow::crc16_ccitt(send_buf, crc_data_len);
    std::memcpy(send_buf + crc_data_len, &crc, sizeof(uint16_t));

    const size_t total_len = crc_data_len + sizeof(uint16_t);
    const esp_err_t err = esp_now_send(dst_mac, send_buf, total_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_now_send error: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

/**
 * @brief Send ESP-NOW packet to target device (first FatigueTester peer)
 * @param device_id Device identifier
 * @param type Message type
 * @param payload Payload data (may be nullptr)
 * @param payload_len Payload length
 * @return true if send successful, false if no target configured
 */
static bool sendPacketToTarget(uint8_t device_id, espnow::MsgType type,
                               const void* payload, uint8_t payload_len)
{
    uint8_t target_mac[6];
    if (!espnow::GetTargetDeviceMac(target_mac)) {
        ESP_LOGW(TAG_, "No target device configured");
        return false;
    }
    return sendPacketTo(target_mac, device_id, type, payload, payload_len);
}

bool espnow::Init(QueueHandle_t event_queue) noexcept
{
    s_proto_event_queue_ = event_queue;
    s_raw_recv_queue_ = xQueueCreate(10, sizeof(RawMsg));

    // Initialize peer store with pre-configured MAC (backward compatibility)
    PeerStore::Init(s_security_, TEST_UNIT_MAC_, DeviceType::FatigueTester, "Pre-configured");

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_, "esp_netif_init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_wifi_set_storage failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_set_channel(WIFI_CHANNEL_, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_wifi_set_channel failed: %s", esp_err_to_name(err));
        return false;
    }

    uint8_t mac_addr[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac_addr);
    ESP_LOGI(TAG_, "Remote Controller MAC (STA): %02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_now_init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_now_register_recv_cb(espnowRecvCb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_now_register_recv_cb failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_now_register_send_cb(espnowSendCb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_now_register_send_cb failed: %s", esp_err_to_name(err));
        return false;
    }

    // Add broadcast peer for pairing discovery
    esp_now_peer_info_t broadcast_peer{};
    std::memcpy(broadcast_peer.peer_addr, BROADCAST_MAC, 6);
    broadcast_peer.channel = WIFI_CHANNEL_;
    broadcast_peer.ifidx = WIFI_IF_STA;
    broadcast_peer.encrypt = false;
    esp_now_add_peer(&broadcast_peer);

    // Add pre-configured peer (backward compatibility)
    if (!IsZeroMac(TEST_UNIT_MAC_)) {
        tryAddEspNowPeer(TEST_UNIT_MAC_);
        ESP_LOGI(TAG_, "Pre-configured test unit: %02X:%02X:%02X:%02X:%02X:%02X",
                 TEST_UNIT_MAC_[0], TEST_UNIT_MAC_[1], TEST_UNIT_MAC_[2],
                 TEST_UNIT_MAC_[3], TEST_UNIT_MAC_[4], TEST_UNIT_MAC_[5]);
    }

    // Add any previously paired peers
    for (size_t i = 0; i < MAX_APPROVED_PEERS; ++i) {
        const auto& peer = s_security_.approved_peers[i];
        if (peer.valid && !IsZeroMac(peer.mac)) {
            tryAddEspNowPeer(peer.mac);
            ESP_LOGI(TAG_, "Restored paired peer: %02X:%02X:%02X:%02X:%02X:%02X (%s)",
                     peer.mac[0], peer.mac[1], peer.mac[2],
                     peer.mac[3], peer.mac[4], peer.mac[5], peer.name);
        }
    }

    xTaskCreate(recvTask, "espnow_recv", 4096, nullptr, 5, nullptr);
    ESP_LOGI(TAG_, "ESP-NOW initialized (protocol v%u)", PROTOCOL_VERSION_);
    ESP_LOGI(TAG_, "Approved peers: %zu", PeerStore::GetPeerCount(s_security_));
    return true;
}

bool espnow::SendDeviceDiscovery() noexcept
{
    return sendPacketTo(BROADCAST_MAC, 0, MsgType::DeviceDiscovery, nullptr, 0);
}

bool espnow::SendConfigRequest(uint8_t device_id) noexcept
{
    return sendPacketToTarget(device_id, MsgType::ConfigRequest, nullptr, 0);
}

bool espnow::SendConfigSet(uint8_t device_id, const void* config_data, size_t config_len) noexcept
{
    if (config_len > MAX_PAYLOAD_SIZE_) {
        ESP_LOGE(TAG_, "Config data too large: %zu", config_len);
        return false;
    }
    return sendPacketToTarget(device_id, MsgType::ConfigSet, config_data, static_cast<uint8_t>(config_len));
}

bool espnow::SendCommand(uint8_t device_id, uint8_t command_id, const void* payload, size_t payload_len) noexcept
{
    uint8_t cmd_buf[espnow::MAX_PAYLOAD_SIZE_];
    cmd_buf[0] = command_id;

    size_t total_payload = 1;
    if (payload != nullptr && payload_len > 0) {
        if (1 + payload_len > MAX_PAYLOAD_SIZE_) {
            ESP_LOGE(TAG_, "Command payload too large: %zu", payload_len);
            return false;
        }
        std::memcpy(cmd_buf + 1, payload, payload_len);
        total_payload = 1 + payload_len;
    }

    return sendPacketToTarget(device_id, MsgType::Command, cmd_buf, static_cast<uint8_t>(total_payload));
}

// ============================================================================
// PAIRING
// ============================================================================

bool espnow::StartPairing() noexcept
{
    if (s_pairing_state_ != PairingState::Idle) {
        ESP_LOGW(TAG_, "Pairing already in progress");
        return false;
    }

    GenerateChallenge(s_my_challenge_);

    PairingRequestPayload req{};
    esp_wifi_get_mac(WIFI_IF_STA, req.requester_mac);
    req.device_type = static_cast<uint8_t>(DeviceType::RemoteController);
    req.expected_peer_type = static_cast<uint8_t>(DeviceType::FatigueTester);
    std::memcpy(req.challenge, s_my_challenge_, CHALLENGE_SIZE);
    req.protocol_version = PROTOCOL_VERSION_;

    if (!sendPacketTo(BROADCAST_MAC, 0, MsgType::PairingRequest, &req, sizeof(req))) {
        ESP_LOGE(TAG_, "Failed to send pairing request");
        return false;
    }

    s_pairing_state_ = PairingState::WaitingForResponse;
    s_pairing_timeout_tick_ = xTaskGetTickCount() + pdMS_TO_TICKS(PAIRING_RESPONSE_TIMEOUT_MS);
    return true;
}

void espnow::CancelPairing() noexcept
{
    if (s_pairing_state_ != PairingState::Idle) {
        s_pairing_state_ = PairingState::Idle;
    }
}

espnow::PairingState espnow::GetPairingState() noexcept
{
    if (s_pairing_state_ == PairingState::WaitingForResponse) {
        if (xTaskGetTickCount() > s_pairing_timeout_tick_) {
            ESP_LOGW(TAG_, "Pairing timed out");
            s_pairing_state_ = PairingState::Failed;
        }
    }
    return s_pairing_state_;
}

SecuritySettings& espnow::GetSecuritySettings() noexcept
{
    return s_security_;
}

bool espnow::IsPeerApproved(const uint8_t mac[6]) noexcept
{
    return PeerStore::IsPeerApproved(s_security_, mac);
}

bool espnow::AddApprovedPeer(const uint8_t mac[6], DeviceType type, const char* name) noexcept
{
    const bool result = PeerStore::AddPeer(s_security_, mac, type, name);
    if (result) {
        tryAddEspNowPeer(mac);
    }
    return result;
}

bool espnow::RemoveApprovedPeer(const uint8_t mac[6]) noexcept
{
    return PeerStore::RemovePeer(s_security_, mac);
}

size_t espnow::GetApprovedPeerCount() noexcept
{
    return PeerStore::GetPeerCount(s_security_);
}

bool espnow::GetTargetDeviceMac(uint8_t mac_out[6]) noexcept
{
    return PeerStore::GetFirstPeerOfType(s_security_, DeviceType::FatigueTester, mac_out);
}

static void handlePairingResponse(const uint8_t* src_mac, const espnow::EspNowPacket& pkt)
{
    if (s_pairing_state_ != espnow::PairingState::WaitingForResponse) {
        return;
    }

    if (pkt.hdr.len < sizeof(PairingResponsePayload)) {
        s_pairing_state_ = espnow::PairingState::Failed;
        return;
    }

    PairingResponsePayload resp;
    std::memcpy(&resp, pkt.payload, sizeof(resp));

    if (resp.device_type != static_cast<uint8_t>(DeviceType::FatigueTester)) {
        return;
    }

    if (!VerifyPairingHmac(s_my_challenge_, CHALLENGE_SIZE, resp.hmac_response)) {
        ESP_LOGE(TAG_, "HMAC verification FAILED - unauthorized device!");
        s_pairing_state_ = espnow::PairingState::Failed;
        return;
    }

    // Add as ESP-NOW peer for sending confirm
    tryAddEspNowPeer(resp.responder_mac);

    uint8_t my_hmac[HMAC_SIZE];
    ComputePairingHmac(resp.challenge, CHALLENGE_SIZE, my_hmac);

    PairingConfirmPayload confirm{};
    esp_wifi_get_mac(WIFI_IF_STA, confirm.confirmer_mac);
    std::memcpy(confirm.hmac_response, my_hmac, HMAC_SIZE);
    confirm.success = 1;

    if (!sendPacketTo(resp.responder_mac, 0, espnow::MsgType::PairingConfirm, &confirm, sizeof(confirm))) {
        s_pairing_state_ = espnow::PairingState::Failed;
        return;
    }

    const bool added = PeerStore::AddPeer(s_security_, resp.responder_mac,
                                         DeviceType::FatigueTester, resp.device_name);
    if (!added) {
        s_pairing_state_ = espnow::PairingState::Failed;
        return;
    }

    s_pairing_state_ = espnow::PairingState::Complete;

    // Notify application layer (optional)
    if (s_proto_event_queue_) {
        espnow::ProtoEvent evt{};
        evt.type = espnow::MsgType::PairingResponse;
        evt.device_id = resp.device_type;
        std::memcpy(evt.src_mac, resp.responder_mac, 6);
        std::memcpy(evt.payload, resp.device_name, sizeof(resp.device_name));
        evt.payload_len = sizeof(resp.device_name);
        xQueueSend(s_proto_event_queue_, &evt, 0);
    }
}

static void handlePairingReject(const uint8_t* src_mac, const espnow::EspNowPacket& pkt)
{
    (void)src_mac;
    if (s_pairing_state_ != espnow::PairingState::WaitingForResponse) {
        return;
    }
    if (pkt.hdr.len < sizeof(PairingRejectPayload)) {
        return;
    }
    // Intentionally don't fail immediately; other devices may respond.
}

static void espnowSendCb(const wifi_tx_info_t* info, esp_now_send_status_t status)
{
    (void)info;
    ESP_LOGD(TAG_, "ESP-NOW send status=%s", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

static void espnowRecvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len)
{
    if (len < 8 || len > static_cast<int>(sizeof(espnow::EspNowPacket))) {
        ESP_LOGW(TAG_, "RX callback: invalid length %d", len);
        return;
    }

    RawMsg msg{};
    msg.len = len;
    std::memcpy(msg.data, data, len);
    std::memcpy(msg.src_mac, info->src_addr, 6);

    BaseType_t hpw = pdFALSE;
    xQueueSendFromISR(s_raw_recv_queue_, &msg, &hpw);
    if (hpw == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void handlePacket(const RawMsg& msg, const uint8_t* data, int len)
{
    if (len < static_cast<int>(sizeof(espnow::EspNowHeader) + sizeof(uint16_t))) {
        return;
    }

    espnow::EspNowHeader hdr{};
    std::memcpy(&hdr, data, sizeof(hdr));
    if (hdr.sync != espnow::SYNC_BYTE_) {
        return;
    }
    if (hdr.version != espnow::PROTOCOL_VERSION_) {
        return;
    }
    if (hdr.len > espnow::MAX_PAYLOAD_SIZE_) {
        return;
    }

    const size_t expected_len = sizeof(espnow::EspNowHeader) + hdr.len + sizeof(uint16_t);
    if (len < static_cast<int>(expected_len)) {
        return;
    }

    const size_t crc_data_len = sizeof(espnow::EspNowHeader) + hdr.len;
    const uint16_t calc_crc = espnow::crc16_ccitt(data, crc_data_len);
    uint16_t recv_crc = 0;
    std::memcpy(&recv_crc, data + crc_data_len, sizeof(uint16_t));
    if (calc_crc != recv_crc) {
        return;
    }

    espnow::EspNowPacket pkt{};
    std::memcpy(&pkt.hdr, data, sizeof(hdr));
    if (hdr.len > 0) {
        std::memcpy(pkt.payload, data + sizeof(hdr), hdr.len);
    }

    const auto type = static_cast<espnow::MsgType>(hdr.type);

    if (type == espnow::MsgType::PairingResponse) {
        handlePairingResponse(msg.src_mac, pkt);
        return;
    }
    if (type == espnow::MsgType::PairingReject) {
        handlePairingReject(msg.src_mac, pkt);
        return;
    }

    // SECURITY GATE: all other messages must come from approved peers
    if (!PeerStore::IsPeerApproved(s_security_, msg.src_mac)) {
        return;
    }

    espnow::ProtoEvent evt{};
    evt.type = type;
    evt.device_id = hdr.device_id;
    evt.sequence_id = hdr.id;
    evt.payload_len = hdr.len;
    std::memcpy(evt.src_mac, msg.src_mac, 6);
    if (hdr.len > 0) {
        std::memcpy(evt.payload, pkt.payload, hdr.len);
    }

    if (s_proto_event_queue_) {
        xQueueSend(s_proto_event_queue_, &evt, 0);
    }
}

/**
 * @brief Receive task - processes packets from ISR queue
 * @param arg Task argument (unused)
 */
static void recvTask(void* arg)
{
    (void)arg;
    RawMsg msg{};
    while (true) {
        if (xQueueReceive(s_raw_recv_queue_, &msg, portMAX_DELAY) == pdTRUE) {
            handlePacket(msg, msg.data, msg.len);
        }
    }
}
