/**
 * @file espnow_protocol.cpp
 * @brief ESP-NOW protocol implementation (compatible with fatigue test unit).
 */

#include "espnow_protocol.hpp"

#include "../config.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_netif.h"
#include "esp_event.h"

#include <cstring>

static const char* TAG_ = "espnow";

static QueueHandle_t s_proto_event_queue_ = nullptr;
static uint8_t s_next_msg_id_ = 1;
static QueueHandle_t s_raw_recv_queue_ = nullptr;

struct RawMsg {
    uint8_t data[sizeof(espnow::EspNowPacket)];
    int len;
};

static void espnowRecvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len);
static void espnowSendCb(const wifi_tx_info_t* info, esp_now_send_status_t status);
static void recvTask(void*);
static void handlePacket(const uint8_t* data, int len);

bool espnow::Init(QueueHandle_t event_queue) noexcept
{
    s_proto_event_queue_ = event_queue;
    s_raw_recv_queue_ = xQueueCreate(10, sizeof(RawMsg));

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

    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, TEST_UNIT_MAC_, 6);
    peer.channel = WIFI_CHANNEL_;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG_, "esp_now_add_peer failed: %s", esp_err_to_name(err));
        return false;
    }

    xTaskCreate(recvTask, "espnow_recv", 4096, nullptr, 5, nullptr);
    ESP_LOGI(TAG_, "ESP-NOW initialized (protocol v%u)", PROTOCOL_VERSION_);
    return true;
}

static bool sendPacket(uint8_t device_id, espnow::MsgType type, const void* payload, uint8_t payload_len) noexcept
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
    const esp_err_t err = esp_now_send(TEST_UNIT_MAC_, send_buf, total_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_now_send error: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool espnow::SendDeviceDiscovery() noexcept
{
    return sendPacket(0, MsgType::DeviceDiscovery, nullptr, 0);
}

bool espnow::SendConfigRequest(uint8_t device_id) noexcept
{
    return sendPacket(device_id, MsgType::ConfigRequest, nullptr, 0);
}

bool espnow::SendConfigSet(uint8_t device_id, const void* config_data, size_t config_len) noexcept
{
    if (config_len > MAX_PAYLOAD_SIZE_) {
        ESP_LOGE(TAG_, "Config data too large: %zu", config_len);
        return false;
    }
    return sendPacket(device_id, MsgType::ConfigSet, config_data, static_cast<uint8_t>(config_len));
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

    return sendPacket(device_id, MsgType::Command, cmd_buf, static_cast<uint8_t>(total_payload));
}

static void espnowSendCb(const wifi_tx_info_t* info, esp_now_send_status_t status)
{
    (void)info;
    ESP_LOGD(TAG_, "ESP-NOW send status=%s", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

static void espnowRecvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len)
{
    (void)info;
    if (len < 8 || len > static_cast<int>(sizeof(espnow::EspNowPacket))) {
        ESP_LOGW(TAG_, "RX callback: invalid length %d", len);
        return;
    }

    RawMsg msg{};
    msg.len = len;
    std::memcpy(msg.data, data, len);

    BaseType_t hpw = pdFALSE;
    xQueueSendFromISR(s_raw_recv_queue_, &msg, &hpw);
    if (hpw == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void handlePacket(const uint8_t* data, int len)
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
        ESP_LOGW(TAG_, "CRC mismatch (calc=0x%04X recv=0x%04X)", calc_crc, recv_crc);
        return;
    }

    espnow::ProtoEvent evt{};
    evt.type = static_cast<espnow::MsgType>(hdr.type);
    evt.device_id = hdr.device_id;
    evt.sequence_id = hdr.id;
    evt.payload_len = hdr.len;
    if (hdr.len > 0) {
        std::memcpy(evt.payload, data + sizeof(espnow::EspNowHeader), hdr.len);
    }

    if (s_proto_event_queue_) {
        xQueueSend(s_proto_event_queue_, &evt, 0);
    }
}

static void recvTask(void* arg)
{
    (void)arg;
    RawMsg msg{};
    while (true) {
        if (xQueueReceive(s_raw_recv_queue_, &msg, portMAX_DELAY) == pdTRUE) {
            handlePacket(msg.data, msg.len);
        }
    }
}
