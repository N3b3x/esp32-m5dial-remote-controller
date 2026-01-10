#include "ui_controller.hpp"

#include "M5Unified.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "../protocol/espnow_protocol.hpp"
#include "../settings.hpp"
#include "../config.hpp"

#include "ui/ui_theme.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <inttypes.h>
#include <cmath>

static const char* TAG_ = "ui";

// Alias for convenience
namespace th = ui::theme;
namespace colors = ui::theme::colors;

// Static menu item definitions matching M5Dial factory demo style
const ui::UiController::CircularMenuItem ui::UiController::kMenuItems_[MENU_COUNT_] = {
    {"Settings", nullptr, ui::assets::CircularIconColors::red, ui::assets::kCircularIcon_settings, 42, 42, Page::Settings},
    {"Find", "Bounds", ui::assets::CircularIconColors::blue, ui::assets::kCircularIcon_bounds, 42, 42, Page::Bounds},
    {"Live", "Counter", ui::assets::CircularIconColors::green, ui::assets::kCircularIcon_live, 42, 42, Page::LiveCounter},
    {"Terminal", nullptr, ui::assets::CircularIconColors::teal, ui::assets::kCircularIcon_terminal, 42, 42, Page::Terminal},
};

ui::UiController::UiController(QueueHandle_t proto_events, Settings* settings) noexcept
    : proto_events_(proto_events)
    , settings_(settings)
    , encoder_(DIAL_ENCODER_PIN_A_, DIAL_ENCODER_PIN_B_, DIAL_ENCODER_PIN_SW_, ENCODER_PULSES_PER_REV_)
{
}

void ui::UiController::Init() noexcept
{
    // Encoder: rotation on A/B; click handled via M5.BtnA or touch.
    (void)encoder_.begin();
    encoder_pos_ = encoder_.getPosition();

    // Create double-buffering canvas sprite (KEY for flicker-free rendering)
    canvas_ = new LGFX_Sprite(&M5.Display);
    if (canvas_ != nullptr) {
        canvas_->setColorDepth(16);
        canvas_->createSprite(SCREEN_SIZE_, SCREEN_SIZE_);
    }

    // Start with display dark for boot animation
    M5.Display.setBrightness(0);
    M5.Display.fillScreen(TFT_BLACK);

    const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    boot_start_ms_ = now_ms;
    
    // Draw boot screen to canvas and push
    if (canvas_ != nullptr) {
        canvas_->fillScreen(TFT_BLACK);
        
        // ConMed™ logo - large centered text
        canvas_->setTextSize(3);
        canvas_->setTextColor(TFT_WHITE);
        canvas_->drawCenterString("ConMed", CENTER_X_, CENTER_Y_ - 30);
        
        // TM superscript
        canvas_->setTextSize(1);
        canvas_->drawString("TM", CENTER_X_ + 58, CENTER_Y_ - 45);
        
        // Subtitle
        canvas_->setTextSize(1);
        canvas_->setTextColor(0xAD55);  // Light gray
        canvas_->drawCenterString("Fatigue Test Unit", CENTER_X_, CENTER_Y_ + 20);
        
        canvas_->pushSprite(0, 0);
    }
    
    // Fade in brightness
    for (int i = 0; i < 128; i++) {
        M5.Display.setBrightness(i);
        vTaskDelay(pdMS_TO_TICKS(4));
    }
    vTaskDelay(pdMS_TO_TICKS(800));  // Hold boot screen

    // Apply saved brightness setting
    if (settings_ != nullptr) {
        M5.Display.setBrightness(settings_->ui.brightness);
    } else {
        M5.Display.setBrightness(128);
    }

    boot_complete_ = true;
    logf_(now_ms, "Boot: UI init");

    // Initialize circular menu
    initCircularMenu_();

    // Kick off initial config request (used as the remote controller's status poll).
    (void)espnow::SendConfigRequest(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_);
    logf_(now_ms, "TX: ConfigRequest dev=%u", fatigue_proto::DEVICE_ID_FATIGUE_TESTER_);

    last_poll_ms_ = now_ms;

    dirty_ = true;
    ESP_LOGI(TAG_, "UI initialized");
}

void ui::UiController::initCircularMenu_() noexcept
{
    // Configure circular menu matching M5Dial factory demo style
    menu_config_.center_x = 240 / 2;
    menu_config_.center_y = 240 / 2;
    menu_config_.icon_radius = 95;       // Distance from center to icons
    menu_config_.selector_radius = 60;   // Distance from center to selector dot
    menu_config_.icon_bg_radius = 22;    // Radius of circular icon background
    menu_config_.icon_selected_offset = 3;
    menu_config_.icon_selected_scale = 1.1f;
    menu_config_.selector_dot_radius = 5;
    menu_config_.selector_color = 0xF3E9;  // Cream/off-white
    menu_config_.anim_duration_ms = kMenuAnimDuration_ms;
    menu_config_.center_touch_radius = 50;
    menu_config_.theme_fg = 0xFA00;
    menu_config_.theme_bg = 0x0000;
    
    menu_selector_.init(menu_config_, MENU_COUNT_);
    
    const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    menu_selector_.setSelectedIndex(0, now_ms, false);  // Start at first item, no animation
}

void ui::UiController::Tick() noexcept
{
    const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);

    // Keepalive/status poll: match esp32_remote_controller behavior.
    // The reference remote uses ConfigRequest as a periodic status/config poll.
    if ((now_ms - last_poll_ms_) >= 1000U) {
        (void)espnow::SendConfigRequest(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_);
        last_poll_ms_ = now_ms;
    }

    handleProtoEvents_(now_ms);
    handleInputs_(now_ms);

    // Loading timer for bounds finding.
    if (bounds_running_ && now_ms >= bounds_until_ms_) {
        bounds_running_ = false;
        dirty_ = true;
    }

    // Render period: faster when animating, slower when static to reduce flicker
    uint32_t period_ms = 250;  // Default: slow refresh
    if (page_ == Page::Landing && menu_selector_.isAnimating(now_ms)) {
        period_ms = 33;  // ~30fps during menu animation
    } else if (page_ == Page::Landing && conn_status_ == ConnStatus::Connecting) {
        period_ms = 500;  // Slow for "Waiting..." animation dots
    }
    
    if (dirty_ || (now_ms - last_render_ms_) > period_ms) {
        draw_(now_ms);
        last_render_ms_ = now_ms;
        dirty_ = false;
    }
}

const char* ui::UiController::pageName_(Page p) noexcept
{
    switch (p) {
        case Page::Landing:
            return "Landing";
        case Page::Settings:
            return "Settings";
        case Page::Bounds:
            return "Bounds";
        case Page::LiveCounter:
            return "Live";
        case Page::Terminal:
            return "Terminal";
        default:
            return "";
    }
}

void ui::UiController::logf_(uint32_t now_ms, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogLine line{};
    line.ms = now_ms;
    vsnprintf(line.text, sizeof(line.text), fmt, args);
    va_end(args);

    log_[log_head_] = line;
    log_head_ = (log_head_ + 1) % LOG_CAPACITY_;
    log_count_ = std::min(LOG_CAPACITY_, log_count_ + 1);
    if (page_ == Page::Terminal && scroll_lines_ == 0) {
        dirty_ = true;
    }
}

void ui::UiController::handleProtoEvents_(uint32_t now_ms) noexcept
{
    espnow::ProtoEvent evt{};
    while (proto_events_ && xQueueReceive(proto_events_, &evt, 0) == pdTRUE) {
        if (evt.device_id != fatigue_proto::DEVICE_ID_FATIGUE_TESTER_) {
            continue;
        }

        // Update connection status on any valid message
        last_rx_ms_ = now_ms;
        if (conn_status_ != ConnStatus::Connected) {
            conn_status_ = ConnStatus::Connected;
            logf_(now_ms, "Connected to fatigue tester");
        }

        switch (evt.type) {
            case espnow::MsgType::StatusUpdate: {
                fatigue_proto::StatusPayload status{};
                if (fatigue_proto::ParseStatus(evt.payload, evt.payload_len, status)) {
                    last_status_ = status;
                    have_status_ = true;
                    logf_(now_ms, "RX: Status cycle=%" PRIu32 " state=%u err=%u", status.cycle_number,
                          static_cast<unsigned>(status.state), static_cast<unsigned>(status.err_code));
                    dirty_ = true;
                }
                break;
            }
            case espnow::MsgType::ConfigResponse: {
                fatigue_proto::ConfigPayload cfg{};
                if (fatigue_proto::ParseConfig(evt.payload, evt.payload_len, cfg)) {
                    last_remote_config_ = cfg;
                    have_remote_config_ = true;
                    logf_(now_ms, "RX: ConfigResponse cycles=%" PRIu32 " t=%" PRIu32 "s dwell=%" PRIu32 "s", cfg.cycle_amount,
                          cfg.time_per_cycle_sec, cfg.dwell_time_sec);
                    dirty_ = true;
                }
                break;
            }
            default:
                // Log other message types succinctly.
                logf_(now_ms, "RX: Msg type=%u len=%u", static_cast<unsigned>(evt.type), static_cast<unsigned>(evt.payload_len));
                break;
        }
    }

    // Check for connection timeout
    if (conn_status_ == ConnStatus::Connected && (now_ms - last_rx_ms_) > kConnTimeout_ms) {
        conn_status_ = ConnStatus::Connecting;
        logf_(now_ms, "Connection timeout - reconnecting");
        dirty_ = true;
    }
}

void ui::UiController::handleInputs_(uint32_t now_ms) noexcept
{
    // Encoder rotation events.
    if (auto* q = encoder_.getEventQueue(); q != nullptr) {
        EC11Encoder::Event evt{};
        while (xQueueReceive(q, &evt, 0) == pdTRUE) {
            if (evt.type == EC11Encoder::EventType::ROTATION) {
                int delta = 0;
                if (evt.direction == EC11Encoder::Direction::CW) {
                    delta = 1;
                } else if (evt.direction == EC11Encoder::Direction::CCW) {
                    delta = -1;
                }
                if (delta != 0) {
                    onRotate_(delta, now_ms);
                }
            }
        }
        encoder_pos_ = encoder_.getPosition();
    }

    // Button click via M5Unified.
    if (M5.BtnA.wasClicked()) {
        onClick_(now_ms);
    }

    // Touch input with gesture detection
    if (M5.Touch.getCount() > 0) {
        const auto& t = M5.Touch.getDetail(0);
        
        // Track touch start for swipe detection
        if (t.wasPressed()) {
            touch_start_x_ = t.x;
            touch_start_y_ = t.y;
            swipe_detected_ = false;
        }
        
        if (t.wasClicked() && !swipe_detected_) {
            onTouchClick_(t.x, t.y, now_ms);
        }
        
        if (t.wasDragStart()) {
            touch_dragging_ = true;
            last_touch_x_ = t.x;
            last_touch_y_ = t.y;
        }
        
        if (touch_dragging_ && t.isDragging()) {
            const int16_t dy = t.y - last_touch_y_;
            last_touch_x_ = t.x;
            last_touch_y_ = t.y;
            onTouchDrag_(dy, now_ms);
            
            // Check for swipe gesture (significant horizontal movement)
            const int16_t total_dx = t.x - touch_start_x_;
            const int16_t total_dy = t.y - touch_start_y_;
            if (!swipe_detected_ && (std::abs(total_dx) > 50 || std::abs(total_dy) > 50)) {
                swipe_detected_ = true;
            }
        }
        
        // Handle swipe on release
        if (t.wasReleased() && swipe_detected_) {
            const int16_t total_dx = t.x - touch_start_x_;
            const int16_t total_dy = t.y - touch_start_y_;
            onSwipe_(total_dx, total_dy, now_ms);
        }
        
        if (t.wasDragged() || t.wasReleased()) {
            touch_dragging_ = false;
        }
    } else {
        touch_dragging_ = false;
    }
}

void ui::UiController::nextPage_(int delta) noexcept
{
    (void)delta;
    // Page cycling was replaced with a main-menu carousel on the Landing page.
}

void ui::UiController::onRotate_(int delta, uint32_t now_ms) noexcept
{
    (void)now_ms;

    auto clamp_add_u32 = [](uint32_t value, int delta_steps, uint32_t step) -> uint32_t {
        const int64_t next = static_cast<int64_t>(value) + static_cast<int64_t>(delta_steps) * static_cast<int64_t>(step);
        if (next < 0) {
            return 0;
        }
        if (next > static_cast<int64_t>(UINT32_MAX)) {
            return UINT32_MAX;
        }
        return static_cast<uint32_t>(next);
    };

    // Page-specific behavior.
    if (page_ == Page::Settings) {
        if (!in_settings_edit_) {
            enterSettings_();
        }

        // If editing a numeric value, rotation changes it.
        if (settings_focus_ == SettingsFocus::List && settings_value_editing_) {
            switch (settings_index_) {
                case 0: // cycles
                    edit_settings_.test_unit.cycle_amount = clamp_add_u32(edit_settings_.test_unit.cycle_amount, delta, 10);
                    break;
                case 1: // time per cycle
                    edit_settings_.test_unit.time_per_cycle_sec = clamp_add_u32(edit_settings_.test_unit.time_per_cycle_sec, delta, 1);
                    break;
                case 2: // dwell
                    edit_settings_.test_unit.dwell_time_sec = clamp_add_u32(edit_settings_.test_unit.dwell_time_sec, delta, 1);
                    break;
                case 4: // bounds_search_velocity_rpm
                    edit_settings_.test_unit.bounds_search_velocity_rpm = std::max(0.0f, edit_settings_.test_unit.bounds_search_velocity_rpm + 0.1f * delta);
                    break;
                case 5:
                    edit_settings_.test_unit.stallguard_min_velocity_rpm = std::max(0.0f, edit_settings_.test_unit.stallguard_min_velocity_rpm + 0.1f * delta);
                    break;
                case 6:
                    edit_settings_.test_unit.stall_detection_current_factor = std::max(0.0f, edit_settings_.test_unit.stall_detection_current_factor + 0.05f * delta);
                    break;
                case 7:
                    edit_settings_.test_unit.bounds_search_accel_rev_s2 = std::max(0.0f, edit_settings_.test_unit.bounds_search_accel_rev_s2 + 0.1f * delta);
                    break;
                case 8: // brightness
                    {
                        int new_brightness = static_cast<int>(edit_settings_.ui.brightness) + delta * 5;
                        edit_settings_.ui.brightness = static_cast<uint8_t>(std::max(10, std::min(255, new_brightness)));
                        // Apply brightness immediately for preview
                        M5.Display.setBrightness(edit_settings_.ui.brightness);
                    }
                    break;
                default:
                    break;
            }
            dirty_ = true;
            return;
        }

        // Otherwise rotation moves focus/selection.
        if (settings_focus_ == SettingsFocus::List) {
            constexpr int kItemCount = 10; // 0..9
            settings_index_ += delta;
            if (settings_index_ < 0) {
                settings_index_ = 0;
                settings_focus_ = SettingsFocus::Back;
            } else if (settings_index_ >= kItemCount) {
                settings_index_ = kItemCount - 1;
                settings_focus_ = SettingsFocus::Save;
            }
        } else if (settings_focus_ == SettingsFocus::Back) {
            if (delta > 0) {
                settings_focus_ = SettingsFocus::Save;
            } else {
                settings_focus_ = SettingsFocus::List;
            }
        } else {
            if (delta < 0) {
                settings_focus_ = SettingsFocus::Back;
            } else {
                settings_focus_ = SettingsFocus::List;
            }
        }

        dirty_ = true;
        return;
    }

    if (page_ == Page::Landing) {
        // Use circular menu selector with smooth animation
        if (delta > 0) {
            menu_selector_.goNext(now_ms);
            playBeep_(1);  // Higher pitch for CW
        } else {
            menu_selector_.goPrev(now_ms);
            playBeep_(0);  // Lower pitch for CCW
        }
        menu_index_ = menu_selector_.getSelectedIndex();
        last_action_ms_ = now_ms;
        dirty_ = true;
        return;
    }

    if (page_ == Page::Terminal && encoder_scroll_mode_) {
        scroll_lines_ = std::max(0, scroll_lines_ + (delta * 2));
        dirty_ = true;
        return;
    }

    if (page_ == Page::LiveCounter && live_popup_mode_ != LivePopupMode::None) {
        handleLivePopupInput_(delta, false, now_ms);
        return;
    }

    // Other pages: rotation is currently reserved for settings editing and terminal scrolling.
}

void ui::UiController::onClick_(uint32_t now_ms) noexcept
{
    if (page_ == Page::Landing) {
        // Enter selected page from circular menu
        const int idx = menu_selector_.getSelectedIndex();
        if (idx >= 0 && idx < MENU_COUNT_) {
            page_ = kMenuItems_[idx].target_page;
            playBeep_(2);  // Button press beep
            logf_(now_ms, "UI: enter %s", kMenuItems_[idx].tag_up);
            if (page_ == Page::Settings) {
                enterSettings_();
            }
            if (page_ == Page::Terminal) {
                scroll_lines_ = 0;
            }
            dirty_ = true;
        }
        return;
    }

    // Terminal: toggle encoder scroll mode.
    if (page_ == Page::Terminal) {
        encoder_scroll_mode_ = !encoder_scroll_mode_;
        logf_(now_ms, "UI: encoder scroll %s", encoder_scroll_mode_ ? "ON" : "OFF");
        dirty_ = true;
        return;
    }

    if (page_ == Page::Settings) {
        if (!in_settings_edit_) {
            enterSettings_();
        }

        if (settings_focus_ == SettingsFocus::Back) {
            settingsBack_();
            dirty_ = true;
            return;
        }
        if (settings_focus_ == SettingsFocus::Save) {
            settingsSave_(now_ms);
            dirty_ = true;
            return;
        }

        // Toggle booleans with click; enter/exit edit for numerics.
        switch (settings_index_) {
            case 3: // bounds method
                edit_settings_.test_unit.bounds_method_stallguard = !edit_settings_.test_unit.bounds_method_stallguard;
                dirty_ = true;
                return;
            case 9: // orientation (flip UI)
                edit_settings_.ui.orientation_flipped = !edit_settings_.ui.orientation_flipped;
                dirty_ = true;
                return;
            default:
                settings_value_editing_ = !settings_value_editing_;
                dirty_ = true;
                return;
        }
    }

    if (page_ == Page::Bounds) {
        // Trigger bounds finding.
        (void)espnow::SendCommand(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_, static_cast<uint8_t>(fatigue_proto::CommandId::RunBoundsFinding), nullptr, 0);
        logf_(now_ms, "TX: Command RunBoundsFinding");
        bounds_running_ = true;
        bounds_until_ms_ = now_ms + 2000;
        dirty_ = true;
        return;
    }

    if (page_ == Page::LiveCounter) {
        // Handle popup if active
        if (live_popup_mode_ != LivePopupMode::None) {
            handleLivePopupInput_(0, true, now_ms);
            return;
        }
        
        // Show appropriate popup based on current state
        const auto test_state = have_status_ ? static_cast<fatigue_proto::TestState>(last_status_.state) : fatigue_proto::TestState::Idle;
        
        switch (test_state) {
            case fatigue_proto::TestState::Idle:
            case fatigue_proto::TestState::Completed:
            case fatigue_proto::TestState::Error:
                live_popup_mode_ = LivePopupMode::StartConfirm;
                live_popup_selection_ = 1; // Default to START
                break;
            case fatigue_proto::TestState::Running:
                live_popup_mode_ = LivePopupMode::RunningActions;
                live_popup_selection_ = 1; // Default to PAUSE
                break;
            case fatigue_proto::TestState::Paused:
                live_popup_mode_ = LivePopupMode::PausedActions;
                live_popup_selection_ = 1; // Default to RESUME
                break;
        }
        playBeep_(2);
        dirty_ = true;
        return;
    }
}

void ui::UiController::onTouchClick_(int16_t x, int16_t y, uint32_t now_ms) noexcept
{
    // Global back button (for non-landing pages).
    if (page_ != Page::Landing) {
        const Rect back_btn{ 10, 8, 70, 34 };
        if (back_btn.contains(x, y)) {
            // Special case: settings back should discard edits.
            if (page_ == Page::Settings) {
                settingsBack_();
            } else {
                page_ = Page::Landing;
            }
            dirty_ = true;
            return;
        }
    }

    // Landing: tap anywhere near center to enter.
    if (page_ == Page::Landing) {
        const int16_t cx = 240 / 2;
        const int16_t cy = 240 / 2;
        const int32_t dx = static_cast<int32_t>(x) - cx;
        const int32_t dy = static_cast<int32_t>(y) - cy;
        if ((dx * dx + dy * dy) < (90 * 90)) {
            onClick_(now_ms);
            return;
        }
    }

    if (page_ == Page::Settings) {
        const int16_t h = 240;
        const Rect back_btn{ 20, static_cast<int16_t>(h - 55), 95, 38 };
        const Rect save_btn{ static_cast<int16_t>(240 - 115), static_cast<int16_t>(h - 55), 95, 38 };
        if (back_btn.contains(x, y)) {
            settingsBack_();
            dirty_ = true;
            return;
        }
        if (save_btn.contains(x, y)) {
            settingsSave_(now_ms);
            dirty_ = true;
            return;
        }
    }

    if (page_ == Page::Bounds) {
        const Rect run_btn{ 40, 130, static_cast<int16_t>(240 - 80), 50 };
        if (run_btn.contains(x, y)) {
            onClick_(now_ms);
            return;
        }
    }

    if (page_ == Page::LiveCounter) {
        const Rect btn{ 40, 160, static_cast<int16_t>(240 - 80), 50 };
        if (btn.contains(x, y)) {
            onClick_(now_ms);
            return;
        }
    }

    if (page_ == Page::Terminal) {
        // Tap top bar to toggle encoder scroll mode.
        const Rect top{ 0, 0, static_cast<int16_t>(240), 50 };
        if (top.contains(x, y)) {
            onClick_(now_ms);
            return;
        }
    }
}

void ui::UiController::onTouchDrag_(int16_t delta_y, uint32_t now_ms) noexcept
{
    (void)now_ms;
    
    if (page_ == Page::Terminal) {
        // Drag up (negative delta_y) should scroll up (older logs).
        const int lines = (-delta_y) / 12;
        if (lines != 0) {
            scroll_lines_ = std::max(0, scroll_lines_ + lines);
            dirty_ = true;
        }
    }
    
    if (page_ == Page::Settings) {
        // Scroll settings list
        settings_scroll_offset_ -= delta_y / 4;
        settings_scroll_offset_ = std::max(0, std::min(settings_scroll_offset_, 6 * kSettingsItemHeight_));
        dirty_ = true;
    }
}

void ui::UiController::onSwipe_(int16_t dx, int16_t dy, uint32_t now_ms) noexcept
{
    (void)dy;
    (void)now_ms;
    
    // Swipe left to go back (on non-landing pages)
    if (page_ != Page::Landing && dx < -60) {
        playBeep_(2);
        if (page_ == Page::Settings) {
            settingsBack_();
        } else {
            page_ = Page::Landing;
        }
        dirty_ = true;
        return;
    }
    
    // Swipe right also goes back (alternative gesture)
    if (page_ != Page::Landing && dx > 60) {
        playBeep_(2);
        if (page_ == Page::Settings) {
            settingsBack_();
        } else {
            page_ = Page::Landing;
        }
        dirty_ = true;
    }
}

void ui::UiController::enterSettings_() noexcept
{
    if (settings_ == nullptr) {
        return;
    }
    edit_settings_ = *settings_;
    in_settings_edit_ = true;
    settings_index_ = 0;
    settings_focus_ = SettingsFocus::List;
    settings_value_editing_ = false;
}

void ui::UiController::settingsBack_() noexcept
{
    // Discard changes, return to landing.
    in_settings_edit_ = false;
    settings_value_editing_ = false;
    page_ = Page::Landing;
}

void ui::UiController::settingsSave_(uint32_t now_ms) noexcept
{
    if (settings_ == nullptr) {
        return;
    }
    *settings_ = edit_settings_;
    (void)SettingsStore::Save(*settings_);
    logf_(now_ms, "UI: settings saved");

    // Apply brightness setting
    M5.Display.setBrightness(settings_->ui.brightness);

    // Push config to test unit.
    const auto payload = fatigue_proto::BuildConfigPayload(*settings_);
    (void)espnow::SendConfigSet(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_, &payload, sizeof(payload));
    logf_(now_ms, "TX: ConfigSet dev=%u", fatigue_proto::DEVICE_ID_FATIGUE_TESTER_);

    in_settings_edit_ = false;
    settings_value_editing_ = false;
    page_ = Page::Landing;
}

void ui::UiController::playBeep_(int type) noexcept
{
    // Buzzer feedback matching M5Dial factory demo
    // type 0: CCW rotation (lower pitch)
    // type 1: CW rotation (higher pitch)
    // type 2: Button press
    switch (type) {
        case 0:  // CCW
            M5.Speaker.tone(6000, 20);
            break;
        case 1:  // CW
            M5.Speaker.tone(7000, 20);
            break;
        case 2:  // Button press
            M5.Speaker.tone(2000, 20);
            break;
        default:
            break;
    }
}

void ui::UiController::drawHeader_(const char* title) noexcept
{
    canvas_->setTextSize(2);
    canvas_->setTextColor(TFT_WHITE);
    canvas_->setCursor(10, 10);
    canvas_->print(title);

    // Page indicator (simple text).
    canvas_->setCursor(10, 35);
    canvas_->setTextSize(1);
    canvas_->printf("%s", pageName_(page_));
}

void ui::UiController::drawBackButton_() noexcept
{
    if (page_ == Page::Landing) {
        return;
    }

    const Rect back_btn{ 10, 8, 70, 34 };
    canvas_->drawRoundRect(back_btn.x, back_btn.y, back_btn.w, back_btn.h, 6, TFT_WHITE);
    canvas_->setTextSize(1);
    canvas_->setTextColor(TFT_WHITE);
    canvas_->setCursor(back_btn.x + 14, back_btn.y + 10);
    canvas_->print("Back");
}

float ui::UiController::easeOutCubic_(float t) noexcept
{
    t = std::max(0.0f, std::min(1.0f, t));
    return 1.0f - std::pow(1.0f - t, 3.0f);
}

void ui::UiController::drawConnectionIndicator_(uint32_t now_ms) noexcept
{
    const int16_t x = 240 - 18;
    const int16_t y = 14;
    
    uint16_t color;
    switch (conn_status_) {
        case ConnStatus::Connected:
            color = 0x07E0;  // Green
            break;
        case ConnStatus::Connecting: {
            // Pulsing yellow
            const float pulse = 0.5f + 0.5f * std::sinf(static_cast<float>(now_ms) * 0.006f);
            const uint8_t g = static_cast<uint8_t>(48 + 15 * pulse);
            color = static_cast<uint16_t>((31 << 11) | (g << 5) | 0);  // Yellow
            break;
        }
        case ConnStatus::Disconnected:
        default:
            color = 0xF800;  // Red
            break;
    }
    
    canvas_->fillCircle(x, y, 5, color);
    canvas_->drawCircle(x, y, 6, TFT_WHITE);
}

void ui::UiController::drawRoundedRect_(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color, bool filled) noexcept
{
    if (filled) {
        canvas_->fillRoundRect(x, y, w, h, r, color);
    } else {
        canvas_->drawRoundRect(x, y, w, h, r, color);
    }
}

void ui::UiController::drawButton_(const Rect& rect, const char* label, bool focused, bool pressed) noexcept
{
    // Button colors
    uint16_t bg_color = TFT_BLACK;
    uint16_t border_color = TFT_WHITE;
    uint16_t text_color = TFT_WHITE;
    
    if (pressed) {
        bg_color = 0x4A69;  // Bright blue
        border_color = 0x6B9F;
        text_color = TFT_WHITE;
    } else if (focused) {
        bg_color = TFT_DARKGREY;
        border_color = 0x6B9F;  // Light blue border
    }
    
    // Draw button
    drawRoundedRect_(rect.x, rect.y, rect.w, rect.h, 8, bg_color, true);
    drawRoundedRect_(rect.x, rect.y, rect.w, rect.h, 8, border_color, false);
    
    // Draw label centered
    canvas_->setTextColor(text_color);
    canvas_->setTextSize(2);
    const int16_t tw = static_cast<int16_t>(canvas_->textWidth(label));
    canvas_->setCursor(rect.x + (rect.w - tw) / 2, rect.y + (rect.h - 14) / 2);
    canvas_->print(label);
}

// ============================================================================
// Modern UI Helper Functions
// ============================================================================

void ui::UiController::drawProgressArc_(int16_t cx, int16_t cy, int16_t r, int16_t thickness,
                                        float progress, uint16_t fg_color, uint16_t bg_color) noexcept
{
    // Background arc (full circle)
    constexpr float start = -90.0f;  // Start at 12 o'clock
    canvas_->fillArc(cx, cy, r, r - thickness, start, start + 360.0f, bg_color);
    
    // Progress arc
    if (progress > 0.001f) {
        const float end = start + 360.0f * std::min(1.0f, progress);
        canvas_->fillArc(cx, cy, r, r - thickness, start, end, fg_color);
    }
}

void ui::UiController::drawModernButton_(int16_t x, int16_t y, int16_t w, int16_t h,
                                         const char* label, bool selected, bool pressed,
                                         uint16_t accent) noexcept
{
    uint16_t bg = pressed ? accent : (selected ? colors::button_active : colors::button_bg);
    uint16_t border = selected ? accent : colors::button_border;
    
    canvas_->fillSmoothRoundRect(x, y, w, h, h/4, bg);
    canvas_->drawRoundRect(x, y, w, h, h/4, border);
    
    canvas_->setTextColor(colors::text_primary);
    canvas_->setTextSize(1);
    const int16_t tw = static_cast<int16_t>(canvas_->textWidth(label));
    canvas_->setCursor(x + (w - tw) / 2, y + (h - 8) / 2);
    canvas_->print(label);
}

void ui::UiController::drawCenteredText_(int16_t cx, int16_t y, const char* text,
                                         uint16_t color, uint8_t size) noexcept
{
    canvas_->setTextSize(size);
    canvas_->setTextColor(color);
    const int16_t tw = static_cast<int16_t>(canvas_->textWidth(text));
    canvas_->setCursor(cx - tw / 2, y);
    canvas_->print(text);
}

void ui::UiController::drawSettingsItem_(int16_t y, int index, const char* label, 
                                         const char* value, bool selected, bool editing) noexcept
{
    const int16_t w = 240 - 40;
    const int16_t x = 20;
    const int16_t h = kSettingsItemHeight_ - 4;
    
    // Card background
    uint16_t bg = selected ? colors::button_active : colors::bg_card;
    canvas_->fillSmoothRoundRect(x, y, w, h, 8, bg);
    
    if (selected) {
        canvas_->drawRoundRect(x, y, w, h, 8, colors::accent_blue);
    }
    
    // Label
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_secondary);
    canvas_->setCursor(x + 12, y + 6);
    canvas_->print(label);
    
    // Value
    canvas_->setTextColor(editing ? colors::accent_yellow : colors::text_primary);
    canvas_->setCursor(x + 12, y + 20);
    canvas_->print(value);
    
    // Edit indicator
    if (editing) {
        canvas_->fillSmoothCircle(x + w - 16, y + h/2, 4, colors::accent_yellow);
    }
}

void ui::UiController::drawCircularMenuSelector_(uint32_t now_ms) noexcept
{
    // Draw the white selector dot with smooth animation
    const Point2D pos = menu_selector_.getSelectorPosition(now_ms);
    const int16_t x = static_cast<int16_t>(pos.x);
    const int16_t y = static_cast<int16_t>(pos.y);
    
    // Draw smooth anti-aliased circle (selector dot)
    canvas_->fillSmoothCircle(x, y, menu_config_.selector_dot_radius, menu_config_.selector_color);
}

void ui::UiController::drawCircularMenuIcons_(uint32_t now_ms) noexcept
{
    const int selected = menu_selector_.getSelectedIndex();
    const bool animating = menu_selector_.isAnimating(now_ms);
    
    for (int i = 0; i < MENU_COUNT_; ++i) {
        const Point2D pos = menu_selector_.getIconPosition(i);
        const int16_t ix = static_cast<int16_t>(pos.x);
        const int16_t iy = static_cast<int16_t>(pos.y);
        
        const auto& item = kMenuItems_[i];
        const bool is_selected = (i == selected);
        
        // Calculate icon background radius (larger for selected)
        int16_t bg_radius = menu_config_.icon_bg_radius;
        if (is_selected && !animating) {
            bg_radius += menu_config_.icon_selected_offset;
        }
        
        // Draw colored circular background
        canvas_->fillSmoothCircle(ix, iy, bg_radius, item.color);
        
        // Draw icon centered on the background
        if (item.icon_data != nullptr) {
            const int16_t icon_x = ix - item.icon_w / 2;
            const int16_t icon_y = iy - item.icon_h / 2;
            canvas_->pushImage(icon_x, icon_y, item.icon_w, item.icon_h, 
                                 item.icon_data, ui::assets::kCircularIconTransparent);
        }
    }
}

void ui::UiController::drawCircularMenuTag_(uint32_t now_ms) noexcept
{
    (void)now_ms;
    const int selected = menu_selector_.getSelectedIndex();
    if (selected < 0 || selected >= MENU_COUNT_) {
        return;
    }
    
    const auto& item = kMenuItems_[selected];
    const int16_t cx = menu_config_.center_x;
    const int16_t cy = menu_config_.center_y;
    
    // Tag color matching selector (cream/off-white)
    canvas_->setTextColor(menu_config_.selector_color);
    canvas_->setTextSize(2);
    
    if (item.tag_up != nullptr) {
        const int16_t tw = static_cast<int16_t>(canvas_->textWidth(item.tag_up));
        if (item.tag_down != nullptr) {
            // Two-line tag
            canvas_->setCursor(cx - tw / 2, cy - 18);
            canvas_->print(item.tag_up);
            
            const int16_t tw2 = static_cast<int16_t>(canvas_->textWidth(item.tag_down));
            canvas_->setCursor(cx - tw2 / 2, cy + 2);
            canvas_->print(item.tag_down);
        } else {
            // Single-line tag
            canvas_->setCursor(cx - tw / 2, cy - 8);
            canvas_->print(item.tag_up);
        }
    }
}

void ui::UiController::draw_(uint32_t now_ms) noexcept
{
    // All rendering goes to canvas for flicker-free display
    if (canvas_ == nullptr) {
        return;
    }
    
    // Clear canvas
    canvas_->fillScreen(TFT_BLACK);

    switch (page_) {
        case Page::Landing:
            drawCircularLanding_(now_ms);
            break;
        case Page::Settings:
            drawSettings_(now_ms);
            break;
        case Page::Bounds:
            drawBounds_(now_ms);
            break;
        case Page::LiveCounter:
            drawLiveCounter_(now_ms);
            break;
        case Page::Terminal:
            drawTerminal_(now_ms);
            break;
        default:
            break;
    }
    
    // Push canvas to display in one operation (eliminates flicker)
    canvas_->pushSprite(0, 0);
}

void ui::UiController::drawCircularLanding_(uint32_t now_ms) noexcept
{
    // Black background (already filled in draw_)
    
    // Draw circular outer ring (subtle)
    const int16_t cx = menu_config_.center_x;
    const int16_t cy = menu_config_.center_y;
    canvas_->drawCircle(cx, cy, 119, 0x2104);  // Subtle ring at edge
    
    // Draw connection indicator
    drawConnectionIndicator_(now_ms);
    
    // Draw selector dot (animated)
    drawCircularMenuSelector_(now_ms);
    
    // Draw icons in circular arrangement
    drawCircularMenuIcons_(now_ms);
    
    // Draw tag/label in center
    drawCircularMenuTag_(now_ms);
    
    // Status hint area (bottom arc) - centered for round screen
    if (have_status_) {
        // Show state indicator centered at bottom
        const char* state_str = "Idle";
        uint16_t state_color = TFT_DARKGREY;
        switch (static_cast<fatigue_proto::TestState>(last_status_.state)) {
            case fatigue_proto::TestState::Running:
                state_str = "Running";
                state_color = 0x07E0;  // Green
                break;
            case fatigue_proto::TestState::Paused:
                state_str = "Paused";
                state_color = 0xFFE0;  // Yellow
                break;
            case fatigue_proto::TestState::Error:
                state_str = "Error";
                state_color = 0xF800;  // Red
                break;
            default:
                break;
        }
        
        // Centered status with cycle count
        canvas_->setTextSize(1);
        char status_buf[32];
        snprintf(status_buf, sizeof(status_buf), "%s #%" PRIu32, state_str, last_status_.cycle_number);
        canvas_->setTextColor(state_color);
        const int16_t tw = static_cast<int16_t>(canvas_->textWidth(status_buf));
        canvas_->setCursor(cx - tw / 2, 240 - 26);
        canvas_->print(status_buf);
    } else if (conn_status_ == ConnStatus::Connecting) {
        // Animated connecting indicator (dots)
        canvas_->setTextSize(1);
        canvas_->setTextColor(0x8410);
        
        // Animate dots based on time
        const int dot_count = ((now_ms / 500) % 4);
        char conn_buf[16] = "Waiting";
        for (int i = 0; i < dot_count; ++i) {
            strncat(conn_buf, ".", sizeof(conn_buf) - strlen(conn_buf) - 1);
        }
        
        const int16_t tw = static_cast<int16_t>(canvas_->textWidth(conn_buf));
        canvas_->setCursor(cx - tw / 2, 240 - 26);
        canvas_->print(conn_buf);
    }
    // When disconnected, connection indicator dot (red) is enough - no text needed
}

void ui::UiController::drawSettings_(uint32_t now_ms) noexcept
{
    (void)now_ms;
    
    if (!in_settings_edit_) {
        enterSettings_();
    }

    // === SMOOTH ANIMATION ===
    const float anim_speed = 0.4f;
    settings_anim_offset_ += (settings_target_offset_ - settings_anim_offset_) * anim_speed;
    settings_target_offset_ = static_cast<float>(settings_index_) * kSettingsItemHeight_;
    
    const int16_t cx = 120;
    const int16_t menu_center_y = 120;
    
    // Determine menu content based on category
    int item_count = 0;
    const char* title = "SETTINGS";
    
    // Array to store labels and values
    const char* labels[8]{};
    char values[8][24]{};
    
    // Main menu labels
    static const char* main_labels[] = {"< Back", "Fatigue Test", "Bounds Finding", "UI Settings"};
    static const char* main_values[] = {"Return to home", "Cycles & timing", "Stall detection", "Display options"};
    
    // Fatigue Test labels
    static const char* fatigue_labels[] = {"< Back", "Cycles", "Time/Cycle", "Dwell Time"};
    
    // Bounds Finding labels
    static const char* bounds_labels[] = {"< Back", "Mode", "Search Speed", "SG Min Vel", "Stall Factor", "Search Accel"};
    
    // UI labels
    static const char* ui_labels[] = {"< Back", "Brightness", "Flip Display"};
    
    switch (settings_category_) {
        case SettingsCategory::Main:
            title = "SETTINGS";
            item_count = 4;
            for (int i = 0; i < item_count; ++i) {
                labels[i] = main_labels[i];
                snprintf(values[i], sizeof(values[i]), "%s", main_values[i]);
            }
            break;
            
        case SettingsCategory::FatigueTest:
            title = "FATIGUE TEST";
            item_count = 4;
            for (int i = 0; i < item_count; ++i) labels[i] = fatigue_labels[i];
            snprintf(values[0], sizeof(values[0]), "Back to settings");
            snprintf(values[1], sizeof(values[1]), "%" PRIu32, edit_settings_.test_unit.cycle_amount);
            snprintf(values[2], sizeof(values[2]), "%" PRIu32 "s", edit_settings_.test_unit.time_per_cycle_sec);
            snprintf(values[3], sizeof(values[3]), "%" PRIu32 "s", edit_settings_.test_unit.dwell_time_sec);
            break;
            
        case SettingsCategory::BoundsFinding:
            title = "BOUNDS";
            item_count = 6;
            for (int i = 0; i < item_count; ++i) labels[i] = bounds_labels[i];
            snprintf(values[0], sizeof(values[0]), "Back to settings");
            snprintf(values[1], sizeof(values[1]), "%s", edit_settings_.test_unit.bounds_method_stallguard ? "StallGuard" : "Encoder");
            snprintf(values[2], sizeof(values[2]), "%.1f rpm", static_cast<double>(edit_settings_.test_unit.bounds_search_velocity_rpm));
            snprintf(values[3], sizeof(values[3]), "%.1f rpm", static_cast<double>(edit_settings_.test_unit.stallguard_min_velocity_rpm));
            snprintf(values[4], sizeof(values[4]), "%.2fx", static_cast<double>(edit_settings_.test_unit.stall_detection_current_factor));
            snprintf(values[5], sizeof(values[5]), "%.1f rev/s²", static_cast<double>(edit_settings_.test_unit.bounds_search_accel_rev_s2));
            break;
            
        case SettingsCategory::UI:
            title = "UI SETTINGS";
            item_count = 3;
            for (int i = 0; i < item_count; ++i) labels[i] = ui_labels[i];
            snprintf(values[0], sizeof(values[0]), "Back to settings");
            snprintf(values[1], sizeof(values[1]), "%d%%", static_cast<int>(edit_settings_.ui.brightness * 100 / 255));
            snprintf(values[2], sizeof(values[2]), "%s", edit_settings_.ui.orientation_flipped ? "Yes" : "No");
            break;
    }
    
    // === DRAW MENU ITEMS (all drawing to canvas_ for flicker-free) ===
    for (int i = 0; i < item_count; ++i) {
        const float item_y_offset = (static_cast<float>(i) * kSettingsItemHeight_) - settings_anim_offset_;
        const int16_t item_y = menu_center_y + static_cast<int16_t>(item_y_offset);
        
        if (item_y < 48 || item_y > 192) continue;
        
        const bool selected = (settings_index_ == i);
        const bool is_category = (settings_category_ == SettingsCategory::Main && i > 0);
        const bool editing = selected && settings_value_editing_;
        
        // Draw item card - inline to use canvas_
        const int16_t card_x = 25;
        const int16_t card_w = 190;
        const int16_t card_h = 36;
        
        // Card background
        uint16_t bg_color = colors::bg_card;
        if (selected) {
            bg_color = editing ? colors::accent_blue : colors::bg_elevated;
        }
        canvas_->fillRoundRect(card_x, item_y - card_h/2, card_w, card_h, 8, bg_color);
        
        // Selection ring
        if (selected) {
            canvas_->drawRoundRect(card_x, item_y - card_h/2, card_w, card_h, 8, 
                                   editing ? TFT_WHITE : colors::accent_orange);
        }
        
        // Label
        canvas_->setTextSize(1);
        canvas_->setTextColor(selected ? TFT_WHITE : colors::text_primary);
        canvas_->setCursor(card_x + 10, item_y - 10);
        canvas_->print(labels[i]);
        
        // Value
        canvas_->setTextColor(selected ? colors::accent_yellow : colors::text_secondary);
        canvas_->setCursor(card_x + 10, item_y + 2);
        canvas_->print(values[i]);
        
        // Draw chevron for categories (Main menu items 1-3)
        if (is_category && selected) {
            canvas_->setTextColor(TFT_WHITE);
            canvas_->setTextSize(1);
            canvas_->setCursor(card_x + card_w - 15, item_y - 4);
            canvas_->print(">");
        }
    }
    
    // === TITLE (drawn after items for clean layering) ===
    canvas_->fillRect(0, 0, 240, 42, lgfx::color565(15, 15, 20));
    canvas_->setTextColor(0xFA7000);
    canvas_->setTextSize(2);
    const int16_t title_w = static_cast<int16_t>(canvas_->textWidth(title));
    canvas_->setCursor(cx - title_w/2, 14);
    canvas_->print(title);
    
    // === SCROLL INDICATOR ===
    if (item_count > 4) {
        const float scroll_frac = static_cast<float>(settings_index_) / static_cast<float>(item_count - 1);
        const int16_t arc_top = 60;
        const int16_t arc_bottom = 180;
        const int16_t ind_y = arc_top + static_cast<int16_t>(scroll_frac * (arc_bottom - arc_top));
        
        const float r = 110.0f;
        const float cy_arc = 120.0f;
        const float dy = static_cast<float>(ind_y) - cy_arc;
        const float dx = std::sqrt(std::max(0.0f, r * r - dy * dy));
        const int16_t ind_x = 120 + static_cast<int16_t>(dx);
        
        canvas_->fillSmoothCircle(ind_x, ind_y, 5, 0xFA7000);
    }
    
    // === BREADCRUMB (show path when in sub-category) ===
    if (settings_category_ != SettingsCategory::Main) {
        canvas_->setTextSize(1);
        canvas_->setTextColor(colors::text_hint);
        canvas_->setCursor(10, 35);
        canvas_->print("Settings >");
    }
}

void ui::UiController::drawBounds_(uint32_t now_ms) noexcept
{
    const int16_t cx = th::CENTER_X;
    const int16_t cy = th::CENTER_Y;
    
    // === RADAR-STYLE BACKGROUND ===
    // Concentric circles for radar effect
    canvas_->drawCircle(cx, cy, 100, colors::bg_card);
    canvas_->drawCircle(cx, cy, 70, colors::bg_card);
    canvas_->drawCircle(cx, cy, 40, colors::bg_card);
    
    // Cross-hair lines
    canvas_->drawLine(cx - 100, cy, cx + 100, cy, colors::bg_card);
    canvas_->drawLine(cx, cy - 100, cx, cy + 100, colors::bg_card);
    
    if (bounds_running_) {
        // === SCANNING ANIMATION ===
        const float scan_angle = std::fmod(static_cast<float>(now_ms) * 0.18f, 360.0f);
        
        // Rotating scan line (like radar sweep)
        const float rad = scan_angle * 3.14159f / 180.0f;
        const int16_t x2 = cx + static_cast<int16_t>(100.0f * std::cos(rad));
        const int16_t y2 = cy + static_cast<int16_t>(100.0f * std::sin(rad));
        canvas_->drawWideLine(cx, cy, x2, y2, 3, colors::accent_green);
        
        // Arc trail (fading scan trail)
        canvas_->fillArc(cx, cy, 105, 98, scan_angle - 45, scan_angle, colors::accent_green);
        
        // Pulsing center dot
        const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now_ms) * 0.01f);
        const int16_t pulse_r = static_cast<int16_t>(8 + 4 * pulse);
        canvas_->fillSmoothCircle(cx, cy, pulse_r, colors::accent_green);
        
        // Status text
        drawCenteredText_(cx, cy + 60, "SCANNING", colors::accent_green, 2);
        
        // Progress countdown
        const uint32_t remain = (bounds_until_ms_ > now_ms) ? (bounds_until_ms_ - now_ms) : 0;
        char time_buf[16];
        snprintf(time_buf, sizeof(time_buf), "%.1fs", static_cast<double>(remain) / 1000.0);
        drawCenteredText_(cx, cy + 85, time_buf, colors::text_secondary, 1);
        
    } else {
        // === IDLE STATE ===
        // Center icon/indicator
        canvas_->fillSmoothCircle(cx, cy, 35, colors::bg_elevated);
        canvas_->drawCircle(cx, cy, 35, colors::accent_blue);
        
        // Icon placeholder (target symbol)
        canvas_->drawLine(cx - 15, cy, cx + 15, cy, colors::accent_blue);
        canvas_->drawLine(cx, cy - 15, cx, cy + 15, colors::accent_blue);
        canvas_->drawCircle(cx, cy, 10, colors::accent_blue);
        
        // "Find Bounds" text
        drawCenteredText_(cx, cy + 55, "FIND BOUNDS", colors::text_primary, 2);
        
        // Hint
        drawCenteredText_(cx, cy + 80, "Press dial to start", colors::text_hint, 1);
    }
    
    // === CORNER ELEMENTS ===
    // Back button (top-left)
    canvas_->fillSmoothRoundRect(8, 8, 50, 26, 8, colors::bg_elevated);
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_secondary);
    canvas_->setCursor(18, 15);
    canvas_->print("< Back");
    
    // Connection indicator (top-right)
    th::drawConnectionDot(240 - 18, 18, conn_status_ == ConnStatus::Connected, now_ms);
}

void ui::UiController::drawLiveCounter_(uint32_t now_ms) noexcept
{
    // Check if popup is active
    if (live_popup_mode_ != LivePopupMode::None) {
        drawLivePopup_(now_ms);
        return;
    }

    const int16_t cx = th::CENTER_X;
    const int16_t cy = th::CENTER_Y;
    const uint32_t cycle = have_status_ ? last_status_.cycle_number : 0;
    const uint32_t target = edit_settings_.test_unit.cycle_amount;
    const auto test_state = have_status_ ? static_cast<fatigue_proto::TestState>(last_status_.state) : fatigue_proto::TestState::Idle;

    // Check pending command timeout
    if (pending_command_id_ != 0 && (now_ms - pending_command_tick_ > 3000)) {
        pending_command_id_ = 0;
    }

    // Determine state color
    uint16_t state_color = colors::state_idle;
    const char* state_text = "IDLE";
    
    switch (test_state) {
        case fatigue_proto::TestState::Running:
            state_color = colors::state_running;
            state_text = "RUNNING";
            break;
        case fatigue_proto::TestState::Paused:
            state_color = colors::state_paused;
            state_text = "PAUSED";
            break;
        case fatigue_proto::TestState::Completed:
            state_color = colors::state_complete;
            state_text = "COMPLETE";
            break;
        case fatigue_proto::TestState::Error:
            state_color = colors::state_error;
            state_text = "ERROR";
            break;
        default:
            break;
    }
    
    if (pending_command_id_ != 0) {
        state_color = colors::text_muted;
        state_text = "SENDING...";
    }

    // === OUTER PROGRESS ARC ===
    // Background arc
    canvas_->fillArc(cx, cy, 115, 100, -90, 270, colors::progress_bg);
    
    // Progress arc based on cycle completion
    const float progress = (target > 0) ? std::min(1.0f, static_cast<float>(cycle) / static_cast<float>(target)) : 0.0f;
    if (progress > 0.001f) {
        const float end_angle = -90.0f + 360.0f * progress;
        canvas_->fillArc(cx, cy, 115, 100, -90, end_angle, state_color);
    }

    // === CENTER CONTENT ===
    // Large cycle count
    char num_buf[16];
    snprintf(num_buf, sizeof(num_buf), "%" PRIu32, cycle);
    canvas_->setTextSize(4);
    canvas_->setTextColor(colors::text_primary);
    const int16_t nw = static_cast<int16_t>(canvas_->textWidth(num_buf));
    canvas_->setCursor(cx - nw / 2, cy - 30);
    canvas_->print(num_buf);
    
    // Target label
    char target_buf[24];
    snprintf(target_buf, sizeof(target_buf), "/ %" PRIu32, target);
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_muted);
    const int16_t tw = static_cast<int16_t>(canvas_->textWidth(target_buf));
    canvas_->setCursor(cx - tw / 2, cy + 8);
    canvas_->print(target_buf);
    
    // State indicator (rounded pill)
    const int16_t pill_w = 80;
    const int16_t pill_h = 20;
    const int16_t pill_x = cx - pill_w / 2;
    const int16_t pill_y = cy + 28;
    canvas_->fillSmoothRoundRect(pill_x, pill_y, pill_w, pill_h, pill_h/2, state_color);
    
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::bg_primary);
    const int16_t sw = static_cast<int16_t>(canvas_->textWidth(state_text));
    canvas_->setCursor(cx - sw / 2, pill_y + 6);
    canvas_->print(state_text);

    // === CORNER ELEMENTS ===
    // Back button (top-left)
    canvas_->fillSmoothRoundRect(8, 8, 50, 26, 8, colors::bg_elevated);
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_secondary);
    canvas_->setCursor(18, 15);
    canvas_->print("< Back");

    // Connection indicator (top-right)
    th::drawConnectionDot(240 - 18, 18, conn_status_ == ConnStatus::Connected, now_ms);

    // === BOTTOM ACTION HINT ===
    const int16_t hint_y = 240 - 28;
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_hint);
    drawCenteredText_(cx, hint_y, "Press dial for actions", colors::text_hint, 1);
    
    // Touch target indicator (subtle arc at bottom)
    canvas_->drawArc(cx, cy, 98, 96, 160, 200, colors::bg_elevated);
}

void ui::UiController::drawLivePopup_(uint32_t now_ms) noexcept
{
    (void)now_ms;
    
    // Semi-transparent overlay effect
    const int16_t cx = 240 / 2;
    const int16_t cy = 240 / 2;
    
    // Popup background
    const int16_t popup_w = 200;
    const int16_t popup_h = 140;
    const int16_t popup_x = cx - popup_w / 2;
    const int16_t popup_y = cy - popup_h / 2;
    
    drawRoundedRect_(popup_x, popup_y, popup_w, popup_h, 12, 0x2104, true);
    drawRoundedRect_(popup_x, popup_y, popup_w, popup_h, 12, 0x6B9F, false);
    
    // Title
    canvas_->setTextSize(2);
    canvas_->setTextColor(TFT_WHITE);
    const char* title = "Actions";
    if (live_popup_mode_ == LivePopupMode::StartConfirm) {
        title = "Start Test?";
    }
    const int16_t title_w = static_cast<int16_t>(canvas_->textWidth(title));
    canvas_->setCursor(cx - title_w / 2, popup_y + 14);
    canvas_->print(title);
    
    // Buttons based on mode
    const int16_t btn_w = 80;
    const int16_t btn_h = 32;
    const int16_t btn_y1 = popup_y + 50;
    const int16_t btn_y2 = popup_y + 90;
    
    if (live_popup_mode_ == LivePopupMode::StartConfirm) {
        // Two buttons: Cancel / Start
        const int16_t btn_x1 = cx - btn_w - 10;
        const int16_t btn_x2 = cx + 10;
        
        const Rect cancel_btn{btn_x1, btn_y1, btn_w, btn_h};
        const Rect start_btn{btn_x2, btn_y1, btn_w, btn_h};
        
        drawButton_(cancel_btn, "Cancel", live_popup_selection_ == 0, false);
        
        // Green start button
        const bool start_sel = (live_popup_selection_ == 1);
        drawRoundedRect_(start_btn.x, start_btn.y, start_btn.w, start_btn.h, 6, 
                         start_sel ? 0x07E0 : 0x0400, true);
        drawRoundedRect_(start_btn.x, start_btn.y, start_btn.w, start_btn.h, 6, 0x07E0, false);
        canvas_->setTextSize(1);
        canvas_->setTextColor(TFT_WHITE);
        canvas_->setCursor(start_btn.x + 22, start_btn.y + 12);
        canvas_->print("START");
        
    } else if (live_popup_mode_ == LivePopupMode::RunningActions) {
        // Three buttons: Back / Pause / Stop
        const int16_t btn_spacing = 8;
        const int16_t total_w = btn_w * 2 + btn_spacing;
        const int16_t btn_x1 = cx - total_w / 2;
        const int16_t btn_x2 = btn_x1 + btn_w + btn_spacing;
        
        const Rect back_btn{btn_x1, btn_y1, btn_w, btn_h};
        const Rect pause_btn{btn_x2, btn_y1, btn_w, btn_h};
        const Rect stop_btn{static_cast<int16_t>(cx - btn_w/2), btn_y2, btn_w, btn_h};
        
        drawButton_(back_btn, "Back", live_popup_selection_ == 0, false);
        
        // Yellow pause
        const bool pause_sel = (live_popup_selection_ == 1);
        drawRoundedRect_(pause_btn.x, pause_btn.y, pause_btn.w, pause_btn.h, 6,
                         pause_sel ? 0xFFE0 : 0x4200, true);
        drawRoundedRect_(pause_btn.x, pause_btn.y, pause_btn.w, pause_btn.h, 6, 0xFFE0, false);
        canvas_->setTextSize(1);
        canvas_->setTextColor(TFT_BLACK);
        canvas_->setCursor(pause_btn.x + 20, pause_btn.y + 12);
        canvas_->print("PAUSE");
        
        // Red stop
        const bool stop_sel = (live_popup_selection_ == 2);
        drawRoundedRect_(stop_btn.x, stop_btn.y, stop_btn.w, stop_btn.h, 6,
                         stop_sel ? 0xF800 : 0x4000, true);
        drawRoundedRect_(stop_btn.x, stop_btn.y, stop_btn.w, stop_btn.h, 6, 0xF800, false);
        canvas_->setTextColor(TFT_WHITE);
        canvas_->setCursor(stop_btn.x + 24, stop_btn.y + 12);
        canvas_->print("STOP");
        
    } else if (live_popup_mode_ == LivePopupMode::PausedActions) {
        // Three buttons: Back / Resume / Stop
        const int16_t btn_spacing = 8;
        const int16_t total_w = btn_w * 2 + btn_spacing;
        const int16_t btn_x1 = cx - total_w / 2;
        const int16_t btn_x2 = btn_x1 + btn_w + btn_spacing;
        
        const Rect back_btn{btn_x1, btn_y1, btn_w, btn_h};
        const Rect resume_btn{btn_x2, btn_y1, btn_w, btn_h};
        const Rect stop_btn{static_cast<int16_t>(cx - btn_w/2), btn_y2, btn_w, btn_h};
        
        drawButton_(back_btn, "Back", live_popup_selection_ == 0, false);
        
        // Green resume
        const bool resume_sel = (live_popup_selection_ == 1);
        drawRoundedRect_(resume_btn.x, resume_btn.y, resume_btn.w, resume_btn.h, 6,
                         resume_sel ? 0x07E0 : 0x0400, true);
        drawRoundedRect_(resume_btn.x, resume_btn.y, resume_btn.w, resume_btn.h, 6, 0x07E0, false);
        canvas_->setTextSize(1);
        canvas_->setTextColor(TFT_WHITE);
        canvas_->setCursor(resume_btn.x + 16, resume_btn.y + 12);
        canvas_->print("RESUME");
        
        // Red stop
        const bool stop_sel = (live_popup_selection_ == 2);
        drawRoundedRect_(stop_btn.x, stop_btn.y, stop_btn.w, stop_btn.h, 6,
                         stop_sel ? 0xF800 : 0x4000, true);
        drawRoundedRect_(stop_btn.x, stop_btn.y, stop_btn.w, stop_btn.h, 6, 0xF800, false);
        canvas_->setTextColor(TFT_WHITE);
        canvas_->setCursor(stop_btn.x + 24, stop_btn.y + 12);
        canvas_->print("STOP");
    }
}

void ui::UiController::handleLivePopupInput_(int delta, bool click, uint32_t now_ms) noexcept
{
    const int max_sel = (live_popup_mode_ == LivePopupMode::StartConfirm) ? 1 : 2;
    
    if (delta != 0) {
        if (delta > 0) {
            live_popup_selection_ = static_cast<uint8_t>((live_popup_selection_ + 1) % (max_sel + 1));
        } else {
            live_popup_selection_ = static_cast<uint8_t>((live_popup_selection_ + max_sel) % (max_sel + 1));
        }
        playBeep_(delta > 0 ? 1 : 0);
        dirty_ = true;
    }
    
    if (click) {
        playBeep_(2);
        
        if (live_popup_mode_ == LivePopupMode::StartConfirm) {
            if (live_popup_selection_ == 0) {
                // Cancel
                live_popup_mode_ = LivePopupMode::None;
            } else {
                // Start
                (void)espnow::SendCommand(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_, 
                    static_cast<uint8_t>(fatigue_proto::CommandId::Start), nullptr, 0);
                pending_command_id_ = 1;
                pending_command_tick_ = now_ms;
                logf_(now_ms, "TX: Start cmd");
                live_popup_mode_ = LivePopupMode::None;
            }
        } else if (live_popup_mode_ == LivePopupMode::RunningActions) {
            if (live_popup_selection_ == 0) {
                // Back
                live_popup_mode_ = LivePopupMode::None;
            } else if (live_popup_selection_ == 1) {
                // Pause
                (void)espnow::SendCommand(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
                    static_cast<uint8_t>(fatigue_proto::CommandId::Pause), nullptr, 0);
                pending_command_id_ = 2;
                pending_command_tick_ = now_ms;
                logf_(now_ms, "TX: Pause cmd");
                live_popup_mode_ = LivePopupMode::None;
            } else {
                // Stop
                (void)espnow::SendCommand(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
                    static_cast<uint8_t>(fatigue_proto::CommandId::Stop), nullptr, 0);
                pending_command_id_ = 4;
                pending_command_tick_ = now_ms;
                logf_(now_ms, "TX: Stop cmd");
                live_popup_mode_ = LivePopupMode::None;
            }
        } else if (live_popup_mode_ == LivePopupMode::PausedActions) {
            if (live_popup_selection_ == 0) {
                // Back
                live_popup_mode_ = LivePopupMode::None;
            } else if (live_popup_selection_ == 1) {
                // Resume
                (void)espnow::SendCommand(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
                    static_cast<uint8_t>(fatigue_proto::CommandId::Resume), nullptr, 0);
                pending_command_id_ = 3;
                pending_command_tick_ = now_ms;
                logf_(now_ms, "TX: Resume cmd");
                live_popup_mode_ = LivePopupMode::None;
            } else {
                // Stop
                (void)espnow::SendCommand(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
                    static_cast<uint8_t>(fatigue_proto::CommandId::Stop), nullptr, 0);
                pending_command_id_ = 4;
                pending_command_tick_ = now_ms;
                logf_(now_ms, "TX: Stop cmd");
                live_popup_mode_ = LivePopupMode::None;
            }
        }
        
        dirty_ = true;
    }
}

void ui::UiController::drawTerminal_(uint32_t now_ms) noexcept
{
    const int16_t cx = 240 / 2;
    const int16_t cy = 240 / 2;
    
    // Circular frame for round screen
    canvas_->drawCircle(cx, cy, 118, 0x2104);
    
    // Header - centered title
    canvas_->setTextSize(2);
    canvas_->setTextColor(TFT_WHITE);
    const char* title = "Log";
    const int16_t tw = static_cast<int16_t>(canvas_->textWidth(title));
    canvas_->setCursor(cx - tw / 2, 8);
    canvas_->print(title);
    
    // Connection indicator in top-right area
    drawConnectionIndicator_(now_ms);
    
    // Back button hint (small, top-left)
    canvas_->setTextSize(1);
    canvas_->setTextColor(0x6B9F);
    canvas_->setCursor(12, 12);
    canvas_->print("<");
    
    // Log area - centered, respecting round edges
    // Use narrower width in middle, wider at center-height
    const int16_t log_top = 38;
    const int16_t log_bottom = 240 - 28;
    const int16_t line_h = 14;
    const int max_lines = (log_bottom - log_top) / line_h;
    
    // Scroll indicator (small dots on right edge)
    if (log_count_ > static_cast<size_t>(max_lines)) {
        const float scroll_pos = 1.0f - static_cast<float>(scroll_lines_) / static_cast<float>(log_count_ - max_lines);
        const int16_t dot_y = log_top + static_cast<int16_t>((log_bottom - log_top - 10) * scroll_pos);
        canvas_->fillSmoothCircle(240 - 12, dot_y, 3, 0x6B9F);
        // Track dots
        canvas_->fillCircle(240 - 12, log_top + 5, 1, 0x4208);
        canvas_->fillCircle(240 - 12, log_bottom - 5, 1, 0x4208);
    }

    // Render log lines - adjust width per line to fit in circle
    int start_from_newest = scroll_lines_;
    int printed = 0;

    canvas_->setTextSize(1);
    
    for (int i = 0; i < max_lines; ++i) {
        const int idx_from_newest = start_from_newest + i;
        if (idx_from_newest >= static_cast<int>(log_count_)) {
            break;
        }
        const size_t newest_index = (log_head_ + LOG_CAPACITY_ - 1) % LOG_CAPACITY_;
        const size_t index = (newest_index + LOG_CAPACITY_ - static_cast<size_t>(idx_from_newest)) % LOG_CAPACITY_;
        const auto& line = log_[index];

        const int16_t y = static_cast<int16_t>(log_bottom - line_h - (printed * line_h));
        
        // Calculate available width at this Y position (circular bounds)
        const float dy = static_cast<float>(y + line_h/2 - cy);
        const float max_radius = 115.0f;
        float half_width = std::sqrt(std::max(0.0f, max_radius * max_radius - dy * dy));
        const int16_t available_width = static_cast<int16_t>(half_width * 2.0f) - 24;  // margin
        const int16_t start_x = cx - static_cast<int16_t>(half_width) + 12;
        
        // Color-code log lines
        uint16_t text_color = 0xAD55;  // Default: light gray
        if (strstr(line.text, "TX:") != nullptr) {
            text_color = 0x6B9F;  // Blue for TX
        } else if (strstr(line.text, "RX:") != nullptr) {
            text_color = 0x07E0;  // Green for RX
        } else if (strstr(line.text, "Error") != nullptr || strstr(line.text, "ERR") != nullptr) {
            text_color = 0xFB20;  // Orange for errors
        } else if (strstr(line.text, "Connected") != nullptr) {
            text_color = 0x07FF;  // Cyan for connection
        }
        
        canvas_->setCursor(start_x, y);
        canvas_->setTextColor(text_color);
        
        // Truncate message to fit available width
        const int max_chars = available_width / 6;  // approx 6px per char at size 1
        char msg_buf[80];
        snprintf(msg_buf, std::min(static_cast<size_t>(max_chars), sizeof(msg_buf)), "%s", line.text);
        canvas_->print(msg_buf);
        
        printed++;
    }
    
    // Bottom status - show entry count
    canvas_->setTextColor(0x4208);
    canvas_->setTextSize(1);
    char count_buf[20];
    snprintf(count_buf, sizeof(count_buf), "%zu entries", log_count_);
    const int16_t ctw = static_cast<int16_t>(canvas_->textWidth(count_buf));
    canvas_->setCursor(cx - ctw / 2, 240 - 18);
    canvas_->print(count_buf);
}
