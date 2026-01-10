/**
 * @file ec11_encoder.cpp
 * @brief EC11 rotary encoder implementation
 */

#include "ec11_encoder.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/gpio_filter.h"
#include <climits>
#include <cstring>

static const char* TAG_EC11 = "EC11Encoder";

// Quadrature decoding table (Gray code to direction)
// State: [A B] -> [old_A old_B new_A new_B]
// 0 = no change, 1 = CW, -1 = CCW
const int8_t EC11Encoder::QUADRATURE_TABLE[16] = {
    0,  // 00 00: no change
    1,  // 00 01: CW
    -1, // 00 10: CCW
    0,  // 00 11: invalid
    -1, // 01 00: CCW
    0,  // 01 01: no change
    0,  // 01 10: invalid
    1,  // 01 11: CW
    1,  // 10 00: CW
    0,  // 10 01: invalid
    0,  // 10 10: no change
    -1, // 10 11: CCW
    0,  // 11 00: invalid
    -1, // 11 01: CCW
    1,  // 11 10: CW
    0   // 11 11: no change
};

EC11Encoder::EC11Encoder(gpio_num_t tra_pin, gpio_num_t trb_pin, gpio_num_t psh_pin,
                         uint8_t pulses_per_rev)
    : tra_pin_(tra_pin), trb_pin_(trb_pin), psh_pin_(psh_pin),
      pulses_per_rev_(pulses_per_rev), position_(0), min_pos_(INT32_MIN),
      max_pos_(INT32_MAX), button_state_(false), button_press_count_(0),
      last_direction_(Direction::NONE), event_queue_(nullptr), isr_queue_(nullptr),
      last_state_(0), detent_counter_(0), last_rotation_time_(0), last_button_time_(0),
      rotation_debounce_ms_(5), button_debounce_ms_(50), task_handle_(nullptr) {
}

EC11Encoder::~EC11Encoder() {
    end();
}

bool EC11Encoder::begin(int32_t min_pos, int32_t max_pos) {
    min_pos_ = min_pos;
    max_pos_ = max_pos;
    
    // Create event queue for user consumption
    event_queue_ = xQueueCreate(10, sizeof(Event));
    if (!event_queue_) {
        ESP_LOGE(TAG_EC11, "Failed to create event queue");
        return false;
    }
    
    // Create ISR queue for internal ISR->task communication
    isr_queue_ = xQueueCreate(20, sizeof(IsrEvent));
    if (!isr_queue_) {
        ESP_LOGE(TAG_EC11, "Failed to create ISR queue");
        vQueueDelete(event_queue_);
        event_queue_ = nullptr;
        return false;
    }
    
    // Configure GPIO pins
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << tra_pin_) | (1ULL << trb_pin_) | (1ULL << psh_pin_);
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_EC11, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        return false;
    }

    // Enable hardware glitch filters per pin (recommended on ESP32-C6)
    auto enable_glitch_filter = [](gpio_num_t pin) {
        gpio_pin_glitch_filter_config_t cfg = {
            .clk_src = GLITCH_FILTER_CLK_SRC_DEFAULT,
            .gpio_num = pin
        };
        gpio_glitch_filter_handle_t handle;
        if (gpio_new_pin_glitch_filter(&cfg, &handle) == ESP_OK) {
            gpio_glitch_filter_enable(handle);
            return true;
        }
        return false;
    };
    bool filter_ok = true;
    filter_ok &= enable_glitch_filter(tra_pin_);
    filter_ok &= enable_glitch_filter(trb_pin_);
    filter_ok &= enable_glitch_filter(psh_pin_);
    if (!filter_ok) {
        ESP_LOGW(TAG_EC11, "Glitch filter not enabled on all pins");
    }
    
    // Install GPIO ISR service (if not already installed)
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_EC11, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        return false;
    }
    // ESP_ERR_INVALID_STATE means service is already installed, which is OK
    
    // Add ISR handlers
    ret = gpio_isr_handler_add(tra_pin_, gpio_isr_handler, this);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_EC11, "Failed to add ISR handler for TRA: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = gpio_isr_handler_add(trb_pin_, gpio_isr_handler, this);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_EC11, "Failed to add ISR handler for TRB: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = gpio_isr_handler_add(psh_pin_, gpio_isr_handler, this);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_EC11, "Failed to add ISR handler for PSH: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Read initial state
    uint8_t tra_state = gpio_get_level(tra_pin_);
    uint8_t trb_state = gpio_get_level(trb_pin_);
    last_state_ = (tra_state << 1) | trb_state;
    
    button_state_ = (gpio_get_level(psh_pin_) == 0); // Active LOW
    
    // Create processing task
    xTaskCreate(encoder_task, "ec11_task", 2048, this, 5, &task_handle_);
    if (!task_handle_) {
        ESP_LOGE(TAG_EC11, "Failed to create encoder task");
        return false;
    }
    
    ESP_LOGI(TAG_EC11, "EC11 encoder initialized: TRA=GPIO%d, TRB=GPIO%d, PSH=GPIO%d",
             tra_pin_, trb_pin_, psh_pin_);
    return true;
}

void EC11Encoder::end() {
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    
    if (event_queue_) {
        vQueueDelete(event_queue_);
        event_queue_ = nullptr;
    }
    
    if (isr_queue_) {
        vQueueDelete(isr_queue_);
        isr_queue_ = nullptr;
    }
    
    gpio_isr_handler_remove(tra_pin_);
    gpio_isr_handler_remove(trb_pin_);
    gpio_isr_handler_remove(psh_pin_);
}

void EC11Encoder::setPosition(int32_t pos) {
    position_ = clampPosition(pos);
    detent_counter_ = 0;  // Reset partial detent accumulation
}

int32_t EC11Encoder::clampPosition(int32_t pos) const {
    if (pos < min_pos_) return min_pos_;
    if (pos > max_pos_) return max_pos_;
    return pos;
}

void IRAM_ATTR EC11Encoder::gpio_isr_handler(void* arg) {
    EC11Encoder* encoder = static_cast<EC11Encoder*>(arg);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Read current encoder state
    uint8_t tra_state = gpio_get_level(encoder->tra_pin_);
    uint8_t trb_state = gpio_get_level(encoder->trb_pin_);
    uint8_t new_state = (tra_state << 1) | trb_state;
    
    // Capture rotation state transition atomically
    uint8_t old_state = encoder->last_state_;
    if (new_state != old_state) {
        // Update last_state_ immediately to prevent duplicate detection
        encoder->last_state_ = new_state;
        
        // Send the state transition to the task for processing
        IsrEvent evt = {
            .old_state = old_state,
            .new_state = new_state,
            .is_button = false,
            .button_pressed = false
        };
        xQueueSendFromISR(encoder->isr_queue_, &evt, &xHigherPriorityTaskWoken);
    }
    
    // Check button state change
    bool button_pressed = (gpio_get_level(encoder->psh_pin_) == 0);
    if (button_pressed != encoder->button_state_) {
        IsrEvent evt = {
            .old_state = 0,
            .new_state = 0,
            .is_button = true,
            .button_pressed = button_pressed
        };
        xQueueSendFromISR(encoder->isr_queue_, &evt, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void EC11Encoder::encoder_task(void* arg) {
    EC11Encoder* encoder = static_cast<EC11Encoder*>(arg);
    IsrEvent isr_evt;
    
    while (1) {
        // Wait for events from ISR queue
        if (xQueueReceive(encoder->isr_queue_, &isr_evt, portMAX_DELAY) == pdTRUE) {
            int64_t now = esp_timer_get_time() / 1000; // Convert to milliseconds
            
            if (isr_evt.is_button) {
                // Process button event with debouncing
                if ((now - encoder->last_button_time_) >= encoder->button_debounce_ms_) {
                    encoder->processButtonChange(isr_evt.button_pressed);
                    encoder->last_button_time_ = now;
                }
            } else {
                // Process rotation event with debouncing
                // Use the captured state transition from ISR (not re-read GPIO)
                if ((now - encoder->last_rotation_time_) >= encoder->rotation_debounce_ms_) {
                    encoder->processQuadratureChangeFromStates(isr_evt.old_state, isr_evt.new_state);
                    encoder->last_rotation_time_ = now;
                }
            }
        }
    }
}

void EC11Encoder::processQuadratureChange(uint8_t new_state) {
    // Legacy function - redirect to new implementation
    processQuadratureChangeFromStates(last_state_, new_state);
}

void EC11Encoder::processQuadratureChangeFromStates(uint8_t old_state, uint8_t new_state) {
    // Build lookup index: [old_A old_B new_A new_B]
    uint8_t lookup_index = (old_state << 2) | new_state;
    
    if (lookup_index >= 16) {
        return; // Invalid state
    }
    
    int8_t direction = QUADRATURE_TABLE[lookup_index];
    
    if (direction != 0) {
        // Accumulate transitions: EC11 encoders have 4 state transitions per detent
        // We only want to emit one event per detent (physical click)
        detent_counter_ += direction;
        
        // Check if we've accumulated a full detent (4 transitions in same direction)
        // Using threshold of 4 for full detent, but also handle direction changes
        const int8_t TRANSITIONS_PER_DETENT = 4;
        
        if (detent_counter_ >= TRANSITIONS_PER_DETENT) {
            // Full CW detent
            detent_counter_ -= TRANSITIONS_PER_DETENT;
            position_++;
            last_direction_ = Direction::CW;
            position_ = clampPosition(position_);
            
            Event evt = {
                .type = EventType::ROTATION,
                .direction = Direction::CW,
                .position = position_,
                .button_pressed = false
            };
            xQueueSend(event_queue_, &evt, 0);
            
        } else if (detent_counter_ <= -TRANSITIONS_PER_DETENT) {
            // Full CCW detent
            detent_counter_ += TRANSITIONS_PER_DETENT;
            position_--;
            last_direction_ = Direction::CCW;
            position_ = clampPosition(position_);
            
            Event evt = {
                .type = EventType::ROTATION,
                .direction = Direction::CCW,
                .position = position_,
                .button_pressed = false
            };
            xQueueSend(event_queue_, &evt, 0);
        }
        // If counter hasn't reached threshold, wait for more transitions
    }
    
    // Note: last_state_ is already updated by ISR, no need to update here
}

void EC11Encoder::processButtonChange(bool pressed) {
    if (pressed != button_state_) {
        button_state_ = pressed;
        
        if (pressed) {
            button_press_count_++;
        }
        
        Event evt = {
            .type = EventType::BUTTON,
            .direction = Direction::NONE,
            .position = position_,
            .button_pressed = pressed
        };
        
        xQueueSend(event_queue_, &evt, 0); // Non-blocking
    }
}

bool EC11Encoder::processEvents(uint32_t timeout_ms) {
    if (!event_queue_) {
        return false;
    }
    
    Event evt;
    TickType_t timeout = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    
    if (xQueueReceive(event_queue_, &evt, timeout)) {
        // Event received - user can process it here or return true to indicate event available
        return true;
    }
    
    return false;
}

