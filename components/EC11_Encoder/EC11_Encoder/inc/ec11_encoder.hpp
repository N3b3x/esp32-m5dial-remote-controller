/**
 * @file ec11_encoder.hpp
 * @brief EC11 rotary encoder driver with quadrature decoding and button handling
 * 
 * Features:
 * - Quadrature decoding using GPIO interrupts
 * - Software debouncing for rotation and button
 * - Direction detection (clockwise/counter-clockwise)
 * - Position tracking with configurable min/max bounds
 * - Push button handling with debouncing
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/**
 * @brief EC11 rotary encoder driver class
 */
class EC11Encoder {
public:
    /**
     * @brief Encoder event types
     */
    enum class EventType {
        ROTATION,   // Encoder rotated
        BUTTON      // Encoder button pressed/released
    };
    
    /**
     * @brief Rotation direction
     */
    enum class Direction {
        NONE = 0,
        CW = 1,     // Clockwise
        CCW = -1    // Counter-clockwise
    };
    
    /**
     * @brief Encoder event structure
     */
    struct Event {
        EventType type;
        Direction direction;  // Valid only for ROTATION events
        int32_t position;      // Current position
        bool button_pressed;   // Valid only for BUTTON events
    };
    
    /**
     * @brief Internal ISR event structure for ISR->task communication
     */
    struct IsrEvent {
        uint8_t old_state;     // State before transition
        uint8_t new_state;     // State after transition
        bool is_button;        // true if button event, false if rotation
        bool button_pressed;   // Button state (valid only if is_button)
    };
    
    /**
     * @brief Constructor
     * @param tra_pin Phase A (CLK) GPIO pin
     * @param trb_pin Phase B (DT) GPIO pin
     * @param psh_pin Push button (SW) GPIO pin
     * @param pulses_per_rev Number of pulses per full rotation (typically 20)
     */
    EC11Encoder(gpio_num_t tra_pin, gpio_num_t trb_pin, gpio_num_t psh_pin,
                uint8_t pulses_per_rev = 20);
    
    /**
     * @brief Destructor
     */
    ~EC11Encoder();
    
    /**
     * @brief Initialize encoder
     * @param min_pos Minimum position value (default: INT32_MIN)
     * @param max_pos Maximum position value (default: INT32_MAX)
     * @return true if successful, false otherwise
     */
    bool begin(int32_t min_pos = INT32_MIN, int32_t max_pos = INT32_MAX);
    
    /**
     * @brief Deinitialize encoder
     */
    void end();
    
    /**
     * @brief Get current encoder position
     * @return Current position
     */
    int32_t getPosition() const { return position_; }
    
    /**
     * @brief Set encoder position
     * @param pos New position (clamped to min/max)
     */
    void setPosition(int32_t pos);
    
    /**
     * @brief Reset position to zero
     */
    void reset() { setPosition(0); }
    
    /**
     * @brief Get rotation direction from last event
     * @return Direction (CW, CCW, or NONE)
     */
    Direction getDirection() const { return last_direction_; }
    
    /**
     * @brief Check if button is currently pressed
     * @return true if pressed, false otherwise
     */
    bool isButtonPressed() const { return button_state_; }
    
    /**
     * @brief Get button press count (since last reset)
     * @return Number of button presses
     */
    uint32_t getButtonPressCount() const { return button_press_count_; }
    
    /**
     * @brief Reset button press count
     */
    void resetButtonPressCount() { button_press_count_ = 0; }
    
    /**
     * @brief Set position bounds
     * @param min_pos Minimum position
     * @param max_pos Maximum position
     */
    void setBounds(int32_t min_pos, int32_t max_pos) {
        min_pos_ = min_pos;
        max_pos_ = max_pos;
        setPosition(position_); // Clamp current position
    }
    
    /**
     * @brief Get event queue handle (for FreeRTOS tasks)
     * @return QueueHandle_t or nullptr if not initialized
     */
    QueueHandle_t getEventQueue() const { return event_queue_; }
    
    /**
     * @brief Process pending events (call from main loop or task)
     * @param timeout_ms Timeout in milliseconds (0 = non-blocking, portMAX_DELAY = blocking)
     * @return true if event was processed, false if timeout
     */
    bool processEvents(uint32_t timeout_ms = 0);

private:
    gpio_num_t tra_pin_;          // Phase A pin
    gpio_num_t trb_pin_;          // Phase B pin
    gpio_num_t psh_pin_;          // Push button pin
    uint8_t pulses_per_rev_;       // Pulses per revolution
    
    int32_t position_;            // Current position
    int32_t min_pos_;             // Minimum position
    int32_t max_pos_;             // Maximum position
    
    bool button_state_;            // Current button state
    uint32_t button_press_count_; // Button press counter
    
    Direction last_direction_;     // Last rotation direction
    
    QueueHandle_t event_queue_;    // Event queue for user consumption
    QueueHandle_t isr_queue_;      // Internal queue for ISR->task communication
    
    // Quadrature state
    volatile uint8_t last_state_;  // Last quadrature state (volatile for ISR access)
    int8_t detent_counter_;        // Accumulates transitions until a full detent (4 transitions)
    int64_t last_rotation_time_; // Last rotation timestamp (for debouncing)
    int64_t last_button_time_;    // Last button change timestamp (for debouncing)
    
    // Debounce timers
    uint32_t rotation_debounce_ms_;
    uint32_t button_debounce_ms_;
    
    // ISR context
    static void IRAM_ATTR gpio_isr_handler(void* arg);
    static void encoder_task(void* arg);
    TaskHandle_t task_handle_;
    
    // Quadrature decoding table (Gray code)
    static const int8_t QUADRATURE_TABLE[16];
    
    // Process quadrature change (legacy - uses last_state_)
    void processQuadratureChange(uint8_t new_state);
    
    // Process quadrature change from captured state transition
    void processQuadratureChangeFromStates(uint8_t old_state, uint8_t new_state);
    
    // Process button change
    void processButtonChange(bool pressed);
    
    // Clamp position to bounds
    int32_t clampPosition(int32_t pos) const;
};

