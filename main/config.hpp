/**
 * @file config.hpp
 * @brief M5Stack Dial (ESP32-S3) remote controller configuration.
 * @details Hardware pin definitions and ESP-NOW configuration constants.
 */

#pragma once

#include <cstdint>
#include "driver/gpio.h"

// ------------- ESPNOW CONFIG -------------

/**
 * @brief Placeholder MAC address of the test unit (receiver)
 * @details Update to match your fatigue test unit MAC address.
 *          Used for backward compatibility with pre-configured peers.
 */
static constexpr uint8_t TEST_UNIT_MAC_[6] = { 0xFC, 0x01, 0x2C, 0xFF, 0xE4, 0xDC };

// ------------- INPUT CONFIG -------------

/**
 * @brief M5Stack Dial rotary encoder pin A (per M5Dial library defines)
 */
static constexpr gpio_num_t DIAL_ENCODER_PIN_A_ = GPIO_NUM_41;

/**
 * @brief M5Stack Dial rotary encoder pin B (per M5Dial library defines)
 */
static constexpr gpio_num_t DIAL_ENCODER_PIN_B_ = GPIO_NUM_40;

/**
 * @brief Encoder push button pin
 * @details The M5Dial Arduino helper doesn't expose this as a discrete GPIO.
 *          If your hardware has a dedicated GPIO for the press switch, set it here.
 *          Otherwise we use M5Unified BtnA / touch as the primary "click" input.
 */
static constexpr gpio_num_t DIAL_ENCODER_PIN_SW_ = GPIO_NUM_NC;

/**
 * @brief Encoder pulses per revolution
 * @details Used for UI feel; component uses detents.
 */
static constexpr uint8_t ENCODER_PULSES_PER_REV_ = 20;
