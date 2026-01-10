#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "settings.hpp"

#include "../protocol/espnow_protocol.hpp"
#include "../protocol/fatigue_protocol.hpp"

#include "../config.hpp"

#include "ec11_encoder.hpp"

#include "ui/assets/menu_icons.hpp"
#include "ui/assets/circular_icons.hpp"
#include "ui/ui_theme.hpp"
#include "smooth_menu.hpp"

#include <M5GFX.h>
#include <lgfx/v1/LGFX_Sprite.hpp>

#include <cstddef>
#include <cstdint>

namespace ui {

class UiController {
public:
    UiController(QueueHandle_t proto_events, Settings* settings) noexcept;
    void Init() noexcept;
    void Tick() noexcept;

private:
    enum class Page : uint8_t {
        Landing = 0,
        Settings,
        Bounds,
        LiveCounter,
        Terminal,
        Count
    };

    struct Rect {
        int16_t x;
        int16_t y;
        int16_t w;
        int16_t h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && py >= y && px < (x + w) && py < (y + h);
        }
    };

    struct LogLine {
        uint32_t ms;
        char text[96];
    };

    // Settings menu category/layer
    enum class SettingsCategory : uint8_t {
        Main = 0,       // Top-level: Fatigue Test, Bounds Finding, UI
        FatigueTest,    // Cycles, Time/Cycle, Dwell Time
        BoundsFinding,  // Mode, Search Speed, SG Min Vel, Stall Factor, Search Accel
        UI              // Brightness, Flip Display
    };

    // Connection status
    enum class ConnStatus : uint8_t {
        Disconnected,
        Connecting,
        Connected,
    };

    QueueHandle_t proto_events_;
    Settings* settings_;

    // Input
    EC11Encoder encoder_;
    int32_t encoder_pos_ = 0;

    // UI state
    Page page_ = Page::Landing;
    bool dirty_ = true;
    uint32_t last_render_ms_ = 0;
    uint32_t last_poll_ms_ = 0;

    // Connection tracking
    ConnStatus conn_status_ = ConnStatus::Disconnected;
    uint32_t last_rx_ms_ = 0;
    static constexpr uint32_t kConnTimeout_ms = 3000;

    // Main menu (Landing) - Circular carousel like M5Dial factory demo
    static constexpr int MENU_COUNT_ = 4;
    int menu_index_ = 0;
    CircularMenuSelector menu_selector_{};
    CircularMenuConfig menu_config_{};
    static constexpr uint32_t kMenuAnimDuration_ms = 300;
    
    // Menu item definitions for circular layout
    struct CircularMenuItem {
        const char* tag_up;
        const char* tag_down;
        uint16_t color;
        const uint16_t* icon_data;
        int16_t icon_w;
        int16_t icon_h;
        Page target_page;
    };
    static const CircularMenuItem kMenuItems_[MENU_COUNT_];

    // Settings editing - layered categories
    Settings edit_settings_{};
    Settings original_settings_{}; // For change detection
    bool in_settings_edit_ = false;
    
    SettingsCategory settings_category_ = SettingsCategory::Main;
    int settings_index_ = 0;
    
    enum class SettingsFocus : uint8_t { List, Back, Save };
    SettingsFocus settings_focus_ = SettingsFocus::List;
    bool settings_value_editing_ = false;
    
    // Settings popup for save confirmation
    enum class SettingsPopupMode : uint8_t {
        None = 0,
        SaveConfirm  // Back pressed: SAVE & EXIT / DISCARD / CANCEL
    };
    SettingsPopupMode settings_popup_mode_ = SettingsPopupMode::None;
    uint8_t settings_popup_selection_ = 0;

    // Bounds finding
    bool bounds_running_ = false;
    uint32_t bounds_until_ms_ = 0;

    // Live Counter - popup support for Start/Pause/Resume/Stop
    enum class LivePopupMode : uint8_t {
        None = 0,
        StartConfirm,     // Idle state: CANCEL / START
        RunningActions,   // Running: BACK / PAUSE / STOP
        PausedActions     // Paused: BACK / RESUME / STOP
    };
    LivePopupMode live_popup_mode_ = LivePopupMode::None;
    uint8_t live_popup_selection_ = 0;
    uint8_t pending_command_id_ = 0;
    uint32_t pending_command_tick_ = 0;
    
    // Brightness control (0-255)
    uint8_t brightness_ = 128;

    // Terminal
    static constexpr size_t LOG_CAPACITY_ = 120;
    LogLine log_[LOG_CAPACITY_]{};
    size_t log_head_ = 0;
    size_t log_count_ = 0;
    int scroll_lines_ = 0; // 0 = bottom
    bool encoder_scroll_mode_ = true;

    // Touch tracking and gestures
    bool touch_dragging_ = false;
    int16_t last_touch_y_ = 0;
    int16_t last_touch_x_ = 0;
    int16_t touch_start_x_ = 0;
    int16_t touch_start_y_ = 0;
    bool swipe_detected_ = false;

    // Settings scrolling and animation
    int settings_scroll_offset_ = 0;
    float settings_anim_offset_ = 0.0f;
    float settings_target_offset_ = 0.0f;
    static constexpr int kSettingsItemHeight_ = 40;
    static constexpr int kSettingsVisibleItems_ = 4;

    // Visual feedback
    uint32_t last_action_ms_ = 0;
    static constexpr uint32_t kFeedbackDuration_ms = 150;

    // Helpers
    void logf_(uint32_t now_ms, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
    void handleProtoEvents_(uint32_t now_ms) noexcept;
    void handleInputs_(uint32_t now_ms) noexcept;
    void onRotate_(int delta, uint32_t now_ms) noexcept;
    void onClick_(uint32_t now_ms) noexcept;
    void onTouchClick_(int16_t x, int16_t y, uint32_t now_ms) noexcept;
    void onTouchDrag_(int16_t delta_y, uint32_t now_ms) noexcept;
    void onSwipe_(int16_t dx, int16_t dy, uint32_t now_ms) noexcept;

    void nextPage_(int delta) noexcept;
    void enterSettings_() noexcept;
    void settingsBack_() noexcept;
    void settingsSave_(uint32_t now_ms) noexcept;

    void draw_(uint32_t now_ms) noexcept;
    void drawHeader_(const char* title) noexcept;
    void drawBackButton_() noexcept;
    void drawConnectionIndicator_(uint32_t now_ms) noexcept;
    void initCircularMenu_() noexcept;
    void drawCircularLanding_(uint32_t now_ms) noexcept;
    void drawCircularMenuSelector_(uint32_t now_ms) noexcept;
    void drawCircularMenuIcons_(uint32_t now_ms) noexcept;
    void drawCircularMenuTag_(uint32_t now_ms) noexcept;
    void playBeep_(int type) noexcept;
    void drawSettings_(uint32_t now_ms) noexcept;
    void drawSettingsItem_(int16_t y, int index, const char* label, const char* value, bool selected, bool editing) noexcept;
    void drawSettingsItem_(int16_t y, float x_offset, int index, const char* label, const char* value, bool selected, bool editing) noexcept;
    void drawSettingsPopup_(uint32_t now_ms) noexcept;
    void drawSettingsValueEditor_(uint32_t now_ms) noexcept;
    void handleSettingsPopupInput_(int delta, bool click, uint32_t now_ms) noexcept;
    bool settingsHaveChanges_() const noexcept;
    int getSettingsItemCount_() const noexcept;
    void handleSettingsValueEdit_(int delta) noexcept;
    void drawBounds_(uint32_t now_ms) noexcept;
    void drawLiveCounter_(uint32_t now_ms) noexcept;
    void drawLivePopup_(uint32_t now_ms) noexcept;
    void handleLivePopupInput_(int delta, bool click, uint32_t now_ms) noexcept;
    void drawTerminal_(uint32_t now_ms) noexcept;

    // Modern UI helpers
    void drawProgressArc_(int16_t cx, int16_t cy, int16_t r, int16_t thickness,
                          float progress, uint16_t fg_color, uint16_t bg_color) noexcept;
    void drawModernButton_(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char* label, bool selected, bool pressed, uint16_t accent) noexcept;
    void drawCenteredText_(int16_t cx, int16_t y, const char* text, uint16_t color, uint8_t size) noexcept;
    
    // Legacy helpers (kept for compatibility)
    void drawRoundedRect_(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color, bool filled) noexcept;
    void drawButton_(const Rect& rect, const char* label, bool focused, bool pressed) noexcept;
    float easeOutCubic_(float t) noexcept;

    static const char* pageName_(Page p) noexcept;

    bool have_status_ = false;
    fatigue_proto::StatusPayload last_status_{};
    bool have_remote_config_ = false;
    fatigue_proto::ConfigPayload last_remote_config_{};
    
    // Double-buffering canvas (eliminates flickering)
    LGFX_Sprite* canvas_ = nullptr;
    static constexpr int16_t SCREEN_SIZE_ = 240;
    static constexpr int16_t CENTER_X_ = 120;
    static constexpr int16_t CENTER_Y_ = 120;
    
    // Boot screen state
    bool boot_complete_ = false;
    uint32_t boot_start_ms_ = 0;
    static constexpr uint32_t BOOT_DURATION_MS_ = 1500;
};

} // namespace ui
