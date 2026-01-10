/**
 * @file config.hpp
 * @brief M5Stack Dial (ESP32-S3) remote controller configuration.
 */

#pragma once

#include <cstdint>
#include "driver/gpio.h"

// ------------- ESPNOW CONFIG -------------

// Placeholder MAC of the test unit (receiver). Update to match your fatigue test unit.
static constexpr uint8_t TEST_UNIT_MAC_[6] = { 0xFC, 0x01, 0x2C, 0xFF, 0xE4, 0xDC };

// ------------- INPUT CONFIG -------------

// M5Stack Dial rotary encoder pins (per M5Dial library defines).
static constexpr gpio_num_t DIAL_ENCODER_PIN_A_ = GPIO_NUM_41;
static constexpr gpio_num_t DIAL_ENCODER_PIN_B_ = GPIO_NUM_40;

// Encoder push button pin.
// NOTE: The M5Dial Arduino helper doesn't expose this as a discrete GPIO.
// If your hardware has a dedicated GPIO for the press switch, set it here.
// Otherwise we use M5Unified BtnA / touch as the primary "click" input.
static constexpr gpio_num_t DIAL_ENCODER_PIN_SW_ = GPIO_NUM_NC;

// Encoder pulses per revolution (used for UI feel; component uses detents).
static constexpr uint8_t ENCODER_PULSES_PER_REV_ = 20;
