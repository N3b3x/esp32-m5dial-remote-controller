/**
 * @file settings.cpp
 * @brief Implementation of persistent settings storage using NVS with CRC32 validation
 */

#include "settings.hpp"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <cstring>

static const char* TAG_ = "settings";

static constexpr const char* NVS_NAMESPACE_ = "m5dial_rc";  ///< NVS namespace for settings
static constexpr const char* NVS_KEY_BLOB_ = "settings";    ///< NVS key for settings blob

/**
 * @brief Compute CRC32-IEEE checksum
 * @param data Data buffer
 * @param len Data length in bytes
 * @return CRC32 checksum value
 */
static uint32_t crc32_ieee(const uint8_t* data, size_t len) noexcept
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            const uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

#pragma pack(push, 1)
/**
 * @brief Settings blob with CRC32 for integrity checking
 */
struct SettingsBlob {
    Settings settings;  ///< Settings data
    uint32_t crc32;     ///< CRC32 checksum of settings
};
#pragma pack(pop)

bool SettingsStore::Init() noexcept
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

Settings SettingsStore::Load() noexcept
{
    Settings defaults{};

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE_, NVS_READONLY, &handle) != ESP_OK) {
        return defaults;
    }

    SettingsBlob blob{};
    size_t required = sizeof(blob);
    esp_err_t err = nvs_get_blob(handle, NVS_KEY_BLOB_, &blob, &required);
    nvs_close(handle);

    if (err != ESP_OK || required != sizeof(blob)) {
        return defaults;
    }

    const uint32_t calc = crc32_ieee(reinterpret_cast<const uint8_t*>(&blob.settings), sizeof(blob.settings));
    if (calc != blob.crc32) {
        ESP_LOGW(TAG_, "settings CRC mismatch; using defaults");
        return defaults;
    }

    return blob.settings;
}

bool SettingsStore::Save(const Settings& settings) noexcept
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    SettingsBlob blob{};
    blob.settings = settings;
    blob.crc32 = crc32_ieee(reinterpret_cast<const uint8_t*>(&blob.settings), sizeof(blob.settings));

    err = nvs_set_blob(handle, NVS_KEY_BLOB_, &blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "Save failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
