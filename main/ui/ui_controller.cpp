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

namespace {
static const char* const kUnitRevPerS2 = reinterpret_cast<const char*>(u8"rev/s\u00B2");
static const char* const kLabelAmaxRevPerS2 = reinterpret_cast<const char*>(u8"AMAX (rev/s\u00B2)");

// UI-safe fallbacks (many M5Dial fonts do not contain the superscript-2 glyph).
static const char* const kUnitRevPerS2Ui = "rev/s^2";
static const char* const kLabelAmaxRevPerS2Ui = "AMAX (rev/s^2)";

static void applyConfigToSettings_(Settings& s, const fatigue_proto::ConfigPayload& c) noexcept
{
    s.test_unit.cycle_amount = c.cycle_amount;
    s.test_unit.oscillation_vmax_rpm = c.oscillation_vmax_rpm;
    s.test_unit.oscillation_amax_rev_s2 = c.oscillation_amax_rev_s2;
    s.test_unit.dwell_time_ms = c.dwell_time_ms;
    // Protocol: 0 = stallguard, 1 = encoder
    s.test_unit.bounds_method_stallguard = (c.bounds_method == 0);

    s.test_unit.bounds_search_velocity_rpm = c.bounds_search_velocity_rpm;
    s.test_unit.stallguard_min_velocity_rpm = c.stallguard_min_velocity_rpm;
    s.test_unit.stallguard_sgt = c.stallguard_sgt;
    s.test_unit.stall_detection_current_factor = c.stall_detection_current_factor;
    s.test_unit.bounds_search_accel_rev_s2 = c.bounds_search_accel_rev_s2;
}
}

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

    updateBoundsState_(now_ms);

    // Render period: faster when animating, slower when static to reduce flicker
    uint32_t period_ms = 250;  // Default: slow refresh
    if (page_ == Page::Landing && menu_selector_.isAnimating(now_ms)) {
        period_ms = 33;  // ~30fps during menu animation
    } else if (page_ == Page::Landing && conn_status_ == ConnStatus::Connecting) {
        period_ms = 500;  // Slow for "Waiting..." animation dots
    } else if (page_ == Page::Terminal && terminal_overscroll_px_ != 0.0f) {
        period_ms = 33;  // ~30fps while spring animation decays
    } else if (page_ == Page::Bounds && (bounds_state_ == BoundsState::Running || bounds_state_ == BoundsState::StartWaitAck || bounds_state_ == BoundsState::StopWaitAck || bounds_state_ == BoundsState::Complete)) {
        period_ms = 33;  // Animate bounds UI
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
            // We just (re)connected; force a resync on the next ConfigResponse so
            // any offline edits do not linger or get partially overwritten.
            pending_machine_resync_ = true;
            // Immediately poll all info on fresh connection
            (void)espnow::SendConfigRequest(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_);
            logf_(now_ms, "Connected to fatigue tester - polling config and status");
        }

        switch (evt.type) {
            case espnow::MsgType::StatusUpdate: {
                fatigue_proto::StatusPayload status{};
                if (fatigue_proto::ParseStatus(evt.payload, evt.payload_len, status)) {
                    last_status_ = status;
                    have_status_ = true;
                    logf_(now_ms, "RX: Status cycle=%" PRIu32 " state=%u err=%u", status.cycle_number,
                          static_cast<unsigned>(status.state), static_cast<unsigned>(status.err_code));

                    // If motor power has been disabled (or bounds expired), previously found bounds
                    // must not be shown as valid. Clear cached bounds results so the UI forces a re-find.
                    if (status.bounds_valid == 0 && bounds_have_result_) {
                        boundsResetResult_();
                        if (bounds_state_ == BoundsState::Complete) {
                            bounds_state_ = BoundsState::Idle;
                            bounds_state_since_ms_ = now_ms;
                        }
                        logf_(now_ms, "UI: cleared cached bounds (invalidated)");
                    }

                    // If bounds UI is running, allow a state transition on real status.
                    const auto st = static_cast<fatigue_proto::TestState>(status.state);
                    if (page_ == Page::Bounds) {
                        if (bounds_state_ == BoundsState::Running && (st == fatigue_proto::TestState::Idle || st == fatigue_proto::TestState::Completed || st == fatigue_proto::TestState::Error)) {
                            // Bounds finding ended (or was stopped). If a BoundsResult arrives it will move us to Complete.
                            if (st == fatigue_proto::TestState::Error) {
                                bounds_state_ = BoundsState::Error;
                                bounds_state_since_ms_ = now_ms;
                                bounds_last_error_code_ = status.err_code;
                            }
                            dirty_ = true;
                        }
                    }
                    dirty_ = true;
                }
                break;
            }
            case espnow::MsgType::ConfigResponse: {
                fatigue_proto::ConfigPayload cfg{};
                if (fatigue_proto::ParseConfig(evt.payload, evt.payload_len, cfg)) {
                    last_remote_config_ = cfg;
                    have_remote_config_ = true;
                      logf_(now_ms, "RX: ConfigResponse cycles=%" PRIu32 " VMAX=%.1f AMAX=%.1f dwell=%.2fs",
                          cfg.cycle_amount, cfg.oscillation_vmax_rpm, cfg.oscillation_amax_rev_s2,
                          static_cast<double>(cfg.dwell_time_ms) / 1000.0);

                    // Apply the received config into our local Settings so the Settings menu
                    // reflects the device state (including StallGuard fields).
                    if (settings_ != nullptr) {
                        applyConfigToSettings_(*settings_, cfg);
                    }

                    // On (re)connect, always resync the Settings editor state from the machine.
                    // This intentionally discards any offline edits that were not sent to the unit.
                    if (pending_machine_resync_) {
                        if (page_ == Page::Settings && in_settings_edit_) {
                            // Cancel any in-progress edit UI.
                            settings_popup_mode_ = SettingsPopupMode::None;
                            settings_popup_selection_ = 0;
                            settings_value_editor_active_ = false;
                            settings_editor_type_ = SettingsEditorValueType::None;

                            // Replace displayed values with machine config and clear dirty edits.
                            if (settings_ != nullptr) {
                                edit_settings_ = *settings_;
                            } else {
                                applyConfigToSettings_(edit_settings_, cfg);
                            }
                            original_settings_ = edit_settings_;
                            settings_dirty_ = false;
                            logf_(now_ms, "UI: resynced settings from machine");
                        }
                        pending_machine_resync_ = false;
                    }

                    // If the user is viewing Settings but has not started editing,
                    // refresh the displayed values. Never override an active value editor
                    // session or any in-progress user edits.
                    if (page_ == Page::Settings && in_settings_edit_) {
                        const bool safe_to_refresh_list = (!settings_dirty_) && (!settings_value_editor_active_) && (settings_popup_mode_ == SettingsPopupMode::None);
                        if (safe_to_refresh_list) {
                            applyConfigToSettings_(edit_settings_, cfg);
                            original_settings_ = edit_settings_;
                        }
                    }

                    dirty_ = true;
                }
                break;
            }
            case espnow::MsgType::CommandAck: {
                logf_(now_ms, "RX: CommandAck");

                // Live Counter: clear the pending "SENDING..." overlay on any ACK that
                // arrives while a command is outstanding. CommandAck is not correlated,
                // so we use timing heuristics similar to the reference remote.
                if (pending_command_id_ != 0U) {
                    if ((now_ms - pending_command_tick_) <= 3000U) {
                        pending_command_id_ = 0U;
                        pending_command_tick_ = 0U;
                        dirty_ = true;
                    }
                }

                // CommandAck has no payload and does not correlate to a specific command.
                // We treat an ACK arriving while waiting as an ACK for our pending action.
                if (page_ == Page::Bounds) {
                    if (bounds_state_ == BoundsState::StartWaitAck) {
                        bounds_state_ = BoundsState::Running;
                        bounds_state_since_ms_ = now_ms;
                        dirty_ = true;
                    } else if (bounds_state_ == BoundsState::StopWaitAck) {
                        bounds_state_ = BoundsState::Idle;
                        bounds_state_since_ms_ = now_ms;
                        dirty_ = true;
                    }
                }
                break;
            }
            case espnow::MsgType::Error: {
                // Error payload is (err_code, at_cycle). Only err_code is relevant for bounds UI.
                uint8_t err_code = 0;
                if (evt.payload_len >= 1) {
                    err_code = evt.payload[0];
                }
                logf_(now_ms, "RX: Error code=%u", static_cast<unsigned>(err_code));
                if (page_ == Page::Bounds) {
                    bounds_state_ = BoundsState::Error;
                    bounds_state_since_ms_ = now_ms;
                    bounds_last_error_code_ = err_code;
                    dirty_ = true;
                }
                break;
            }
            case espnow::MsgType::BoundsResult: {
                fatigue_proto::BoundsResultPayload br{};
                if (fatigue_proto::ParseBoundsResult(evt.payload, evt.payload_len, br)) {
                    bounds_have_result_ = (br.ok != 0);
                    bounds_bounded_ = (br.bounded != 0);
                    bounds_cancelled_ = (br.cancelled != 0);
                    bounds_min_deg_ = br.min_degrees_from_center;
                    bounds_max_deg_ = br.max_degrees_from_center;
                    bounds_global_min_deg_ = br.global_min_degrees;
                    bounds_global_max_deg_ = br.global_max_degrees;

                    logf_(now_ms, "RX: BoundsResult ok=%u bounded=%u min=%.2f max=%.2f", static_cast<unsigned>(br.ok), static_cast<unsigned>(br.bounded),
                          static_cast<double>(bounds_min_deg_), static_cast<double>(bounds_max_deg_));

                    if (page_ == Page::Bounds) {
                        bounds_state_ = BoundsState::Complete;
                        bounds_state_since_ms_ = now_ms;
                        dirty_ = true;
                    }
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
        // Clear stale status data - don't show old "Idle" or "Running" when disconnected
        have_status_ = false;
        have_remote_config_ = false;
        last_status_ = {};
        last_remote_config_ = {};
        logf_(now_ms, "Connection timeout - cleared stale status data");
        dirty_ = true;
    }
}

void ui::UiController::boundsResetResult_() noexcept
{
    bounds_have_result_ = false;
    bounds_bounded_ = false;
    bounds_cancelled_ = false;
    bounds_min_deg_ = 0.0f;
    bounds_max_deg_ = 0.0f;
    bounds_global_min_deg_ = 0.0f;
    bounds_global_max_deg_ = 0.0f;
    bounds_last_error_code_ = 0;
}

void ui::UiController::boundsStart_(uint32_t now_ms) noexcept
{
    boundsResetResult_();

    (void)espnow::SendCommand(
        fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
        static_cast<uint8_t>(fatigue_proto::CommandId::RunBoundsFinding),
        nullptr,
        0);
    logf_(now_ms, "TX: Command RunBoundsFinding (awaiting ACK)");

    bounds_state_ = BoundsState::StartWaitAck;
    bounds_state_since_ms_ = now_ms;
    bounds_ack_deadline_ms_ = now_ms + 1500;
    dirty_ = true;
}

void ui::UiController::boundsStop_(uint32_t now_ms) noexcept
{
    (void)espnow::SendCommand(
        fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
        static_cast<uint8_t>(fatigue_proto::CommandId::Stop),
        nullptr,
        0);
    logf_(now_ms, "TX: Command Stop (cancel bounds; awaiting ACK)");

    bounds_state_ = BoundsState::StopWaitAck;
    bounds_state_since_ms_ = now_ms;
    bounds_ack_deadline_ms_ = now_ms + 1500;
    dirty_ = true;
}

void ui::UiController::updateBoundsState_(uint32_t now_ms) noexcept
{
    if (page_ != Page::Bounds) {
        return;
    }

    // ACK timeouts
    if ((bounds_state_ == BoundsState::StartWaitAck || bounds_state_ == BoundsState::StopWaitAck) && now_ms >= bounds_ack_deadline_ms_) {
        bounds_state_ = BoundsState::Error;
        bounds_state_since_ms_ = now_ms;
        // 0 => "no-ack" (UI-local meaning)
        bounds_last_error_code_ = 0;
        logf_(now_ms, "Bounds: ACK timeout");
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

    // Button actions via M5Unified.
    // In the Settings value editor: long-press cycles step size (for float or U32 editors) instead of finishing.
    if (page_ == Page::Settings && settings_value_editor_active_) {
        if ((settings_editor_type_ == SettingsEditorValueType::F32 || settings_editor_type_ == SettingsEditorValueType::U32) && M5.BtnA.wasReleasedAfterHold()) {
            cycleSettingsEditorStep_();
            playBeep_(1);
            dirty_ = true;
            return;
        }
    }
    
    // Quick Settings: long-press cycles step size when editing (F32 or U32)
    if (page_ == Page::LiveCounter && live_popup_mode_ == LivePopupMode::QuickSettings) {
        if (quick_settings_editing_ && (quick_editor_type_ == QuickEditorType::F32 || quick_editor_type_ == QuickEditorType::U32) && M5.BtnA.wasReleasedAfterHold()) {
            cycleQuickSettingsStep_();
            playBeep_(1);
            dirty_ = true;
            return;
        }
    }
    
    // LiveCounter: long-press opens Quick Settings (only during Running/Paused)
    if (page_ == Page::LiveCounter && live_popup_mode_ == LivePopupMode::None) {
        if (M5.BtnA.wasReleasedAfterHold()) {
            const bool use_status = (conn_status_ == ConnStatus::Connected && have_status_);
            const auto test_state = use_status ? static_cast<fatigue_proto::TestState>(last_status_.state) : fatigue_proto::TestState::Idle;
            
            if (test_state == fatigue_proto::TestState::Running || test_state == fatigue_proto::TestState::Paused) {
                // Sync edit_settings_ from machine config before opening Quick Settings
                if (settings_ != nullptr) {
                    edit_settings_ = *settings_;
                }
                // Open quick settings
                live_popup_mode_ = LivePopupMode::QuickSettings;
                quick_settings_index_ = 0;
                quick_settings_editing_ = false;
                quick_settings_confirm_popup_ = false;
                playBeep_(2);
                dirty_ = true;
                return;
            }
        }
    }

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
            touch_start_ms_ = now_ms;
            swipe_detected_ = false;
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

        // Robust click detection: treat a press+release with minimal movement as a click.
        // This is more reliable than relying solely on M5Unified's wasClicked(), which can
        // be missed if the touch jitters slightly.
        if (t.wasReleased() && !swipe_detected_) {
            const int16_t dx = t.x - touch_start_x_;
            const int16_t dy = t.y - touch_start_y_;
            const int32_t dist2 = static_cast<int32_t>(dx) * dx + static_cast<int32_t>(dy) * dy;
            const uint32_t held_ms = now_ms - touch_start_ms_;

            if (dist2 <= (12 * 12) && held_ms <= 500U) {
                onTouchClick_(t.x, t.y, now_ms);
            }
        }

        // Only clear dragging state on release; clearing on every `wasDragged()` makes
        // gesture handling sporadic.
        if (t.wasReleased()) {
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

    // Page-specific behavior.
    if (page_ == Page::Settings) {
        if (!in_settings_edit_) {
            enterSettings_();
        }

        // Popup has priority.
        if (settings_popup_mode_ != SettingsPopupMode::None) {
            handleSettingsPopupInput_(delta, false, now_ms);
            dirty_ = true;
            return;
        }

        // Dedicated value editor has priority.
        if (settings_value_editor_active_) {
            handleSettingsValueEdit_(delta);
            dirty_ = true;
            return;
        }

        // Otherwise rotation moves selection within current category.
        const int item_count = getSettingsItemCount_();
        settings_index_ = std::clamp(settings_index_ + delta, 0, item_count - 1);

        // Persist last non-back selection per submenu.
        if (settings_index_ > 0) {
            switch (settings_category_) {
                case SettingsCategory::FatigueTest: settings_last_fatigue_index_ = settings_index_; break;
                case SettingsCategory::BoundsFinding: settings_last_bounds_index_ = settings_index_; break;
                case SettingsCategory::UI: settings_last_ui_index_ = settings_index_; break;
                case SettingsCategory::Main: break;
            }
        }
        dirty_ = true;
        return;
    }

    if (page_ == Page::Bounds) {
        if (delta != 0) {
            bounds_focus_ = (bounds_focus_ == BoundsFocus::Action) ? BoundsFocus::Back : BoundsFocus::Action;
            dirty_ = true;
        }
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

    if (page_ == Page::Terminal) {
        // Always scroll on encoder rotation - no mode toggle needed.
        // Match the earlier UX:
        //   CW scrolls UP (newer, toward bottom)
        //   CCW scrolls DOWN (older, toward top)
        constexpr int16_t log_top = 38;
        constexpr int16_t log_bottom = 240 - 28;
        constexpr int16_t line_h = 14;
        const int max_lines = (log_bottom - log_top) / line_h;
        const int max_scroll = std::max(0, static_cast<int>(log_count_) - max_lines);

        // scroll_lines_ is "lines away from newest".
        const int desired = scroll_lines_ - (delta * 2);
        if (desired < 0) {
            scroll_lines_ = 0;
            terminal_overscroll_px_ = std::max(terminal_overscroll_px_, 8.0f);
        } else if (desired > max_scroll) {
            scroll_lines_ = max_scroll;
            terminal_overscroll_px_ = std::min(terminal_overscroll_px_, -8.0f);
        } else {
            scroll_lines_ = desired;
        }
        dirty_ = true;
        return;
    }

    if (page_ == Page::LiveCounter && live_popup_mode_ == LivePopupMode::QuickSettings) {
        handleQuickSettingsInput_(delta, false, now_ms);
        return;
    }

    if (page_ == Page::LiveCounter && live_popup_mode_ == LivePopupMode::None) {
        if (delta != 0) {
            live_focus_ = (live_focus_ == LiveFocus::Actions) ? LiveFocus::Back : LiveFocus::Actions;
            dirty_ = true;
        }
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
                terminal_overscroll_px_ = 0.0f;
            }
            dirty_ = true;
        }
        return;
    }

    // Terminal: button click goes back to landing
    if (page_ == Page::Terminal) {
        page_ = Page::Landing;
        playBeep_(2);
        logf_(now_ms, "UI: back to landing");
        dirty_ = true;
        return;
    }

    if (page_ == Page::Settings) {
        if (!in_settings_edit_) {
            enterSettings_();
        }

        // Popup click.
        if (settings_popup_mode_ != SettingsPopupMode::None) {
            handleSettingsPopupInput_(0, true, now_ms);
            dirty_ = true;
            return;
        }

        // Value editor click: exit editor and (if changed) confirm keep/discard.
        if (settings_value_editor_active_) {
            playBeep_(2);
            if (settingsEditorHasChange_()) {
                settings_popup_mode_ = SettingsPopupMode::ValueChangeConfirm;
                settings_popup_selection_ = 0; // default KEEP
            } else {
                settings_value_editor_active_ = false;
                settings_editor_type_ = SettingsEditorValueType::None;
            }
            dirty_ = true;
            return;
        }

        playBeep_(2);

        // Index 0 is always "< Back".
        if (settings_index_ == 0) {
            if (settings_category_ == SettingsCategory::Main) {
                settingsBack_();
            } else {
                settings_category_ = SettingsCategory::Main;
                settings_index_ = settings_return_main_index_;
            }
            dirty_ = true;
            return;
        }

        // Main: enter sub-category.
        if (settings_category_ == SettingsCategory::Main) {
            settings_return_main_index_ = settings_index_;
            switch (settings_index_) {
                case 1: settings_category_ = SettingsCategory::FatigueTest; break;
                case 2: settings_category_ = SettingsCategory::BoundsFinding; break;
                case 3: settings_category_ = SettingsCategory::UI; break;
                default: break;
            }
            // Restore last selection inside the submenu (avoid jumping to "< Back").
            switch (settings_category_) {
                case SettingsCategory::FatigueTest: settings_index_ = std::max(1, settings_last_fatigue_index_); break;
                case SettingsCategory::BoundsFinding: settings_index_ = std::max(1, settings_last_bounds_index_); break;
                case SettingsCategory::UI: settings_index_ = std::max(1, settings_last_ui_index_); break;
                default: settings_index_ = 1; break;
            }
            // Clamp to the submenu bounds in case menu size changed.
            settings_index_ = std::min(settings_index_, getSettingsItemCount_() - 1);
            dirty_ = true;
            return;
        }

        // Sub-categories: always enter dedicated value editor.
        beginSettingsValueEditor_();
        dirty_ = true;
        return;
    }

    if (page_ == Page::Bounds) {
        if (bounds_focus_ == BoundsFocus::Back) {
            page_ = Page::Landing;
            dirty_ = true;
            return;
        }

        // Action button (Start/Stop depending on state)
        if (bounds_state_ == BoundsState::Idle || bounds_state_ == BoundsState::Complete || bounds_state_ == BoundsState::Error) {
            boundsStart_(now_ms);
            return;
        }

        if (bounds_state_ == BoundsState::Running) {
            boundsStop_(now_ms);
            return;
        }

        // If already waiting for ACK, ignore additional presses.
        playBeep_(1);
        return;
    }

    if (page_ == Page::LiveCounter) {
        // Handle QuickSettings popup separately
        if (live_popup_mode_ == LivePopupMode::QuickSettings) {
            handleQuickSettingsInput_(0, true, now_ms);
            return;
        }
        
        // Encoder navigation: allow selecting Back vs Actions without touch.
        if (live_popup_mode_ == LivePopupMode::None && live_focus_ == LiveFocus::Back) {
            // If disconnected, allow direct exit without any popup
            if (conn_status_ != ConnStatus::Connected) {
                page_ = Page::Landing;
                playBeep_(2);
                dirty_ = true;
                return;
            }
            
            const bool use_status = have_status_;
            const auto test_state = use_status ? static_cast<fatigue_proto::TestState>(last_status_.state) : fatigue_proto::TestState::Idle;

            // Safety: do not exit while running/paused. Instead, open the actions popup.
            if (test_state == fatigue_proto::TestState::Running) {
                live_popup_mode_ = LivePopupMode::RunningActions;
                live_popup_selection_ = 0; // Back
                playBeep_(2);
                dirty_ = true;
                return;
            }
            if (test_state == fatigue_proto::TestState::Paused) {
                live_popup_mode_ = LivePopupMode::PausedActions;
                live_popup_selection_ = 0; // Back
                playBeep_(2);
                dirty_ = true;
                return;
            }

            page_ = Page::Landing;
            playBeep_(2);
            dirty_ = true;
            return;
        }

        // Handle popup if active
        if (live_popup_mode_ != LivePopupMode::None) {
            handleLivePopupInput_(0, true, now_ms);
            return;
        }
        
        // If disconnected, allow direct exit without showing action popup
        if (conn_status_ != ConnStatus::Connected) {
            page_ = Page::Landing;
            playBeep_(2);
            dirty_ = true;
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
    if (page_ != Page::Landing && page_ != Page::Bounds) {
        // LiveCounter: place Back inside circular safe area (top corners are clipped).
        const Rect back_btn = (page_ == Page::LiveCounter)
            ? Rect{ 76, 10, 88, 30 }
            : Rect{ 10, 8, 70, 34 };
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
        // Slightly higher to avoid bottom-edge clipping on the round display.
        const Rect back_btn{ 18, 186, 64, 30 };
        const Rect action_btn{ 90, 186, 132, 30 };
        if (action_btn.contains(x, y)) {
            bounds_focus_ = BoundsFocus::Action;
            onClick_(now_ms);
            return;
        }
        if (back_btn.contains(x, y)) {
            bounds_focus_ = BoundsFocus::Back;
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
            constexpr int16_t log_top = 38;
            constexpr int16_t log_bottom = 240 - 28;
            constexpr int16_t line_h = 14;
            const int max_lines = (log_bottom - log_top) / line_h;
            const int max_scroll = std::max(0, static_cast<int>(log_count_) - max_lines);

            const int desired = scroll_lines_ + lines;
            if (desired < 0) {
                scroll_lines_ = 0;
                terminal_overscroll_px_ = std::max(terminal_overscroll_px_, 8.0f);
            } else if (desired > max_scroll) {
                scroll_lines_ = max_scroll;
                terminal_overscroll_px_ = std::min(terminal_overscroll_px_, -8.0f);
            } else {
                scroll_lines_ = desired;
            }
            dirty_ = true;
        }
    }
    
    if (page_ == Page::Settings) {
        // Scroll settings list
        settings_scroll_offset_ -= delta_y / 4;
        settings_scroll_offset_ = std::max(0, std::min(settings_scroll_offset_, 7 * kSettingsItemHeight_));
        dirty_ = true;
    }
}

void ui::UiController::onSwipe_(int16_t dx, int16_t dy, uint32_t now_ms) noexcept
{
    (void)dy;
    (void)now_ms;

    // Live Counter: while actively running/paused, avoid accidental exits via swipe.
    // Back button remains the explicit exit path.
    // Only check status if connected - don't use stale data when disconnected
    if (page_ == Page::LiveCounter && conn_status_ == ConnStatus::Connected && have_status_) {
        const auto st = static_cast<fatigue_proto::TestState>(last_status_.state);
        if (st == fatigue_proto::TestState::Running || st == fatigue_proto::TestState::Paused) {
            return;
        }
    }
    
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
    original_settings_ = edit_settings_;
    in_settings_edit_ = true;
    settings_dirty_ = false;
    settings_index_ = 0;
    settings_category_ = SettingsCategory::Main;
    settings_return_main_index_ = 0;
    settings_focus_ = SettingsFocus::List;
    settings_value_editing_ = false;

    settings_last_fatigue_index_ = 1;
    settings_last_bounds_index_ = 1;
    settings_last_ui_index_ = 1;

    settings_popup_mode_ = SettingsPopupMode::None;
    settings_popup_selection_ = 0;

    settings_value_editor_active_ = false;
    settings_editor_category_ = SettingsCategory::Main;
    settings_editor_index_ = 0;
    settings_editor_type_ = SettingsEditorValueType::None;

    // Reset animation state so the list starts stable.
    settings_anim_offset_ = 0.0f;
    settings_target_offset_ = 0.0f;
}

int ui::UiController::getSettingsItemCount_() const noexcept
{
    switch (settings_category_) {
        case SettingsCategory::Main: return 4;
        case SettingsCategory::FatigueTest: return 5;     // Back, Cycles, VMAX, AMAX, Dwell
        case SettingsCategory::BoundsFinding: return 7;   // Back + 6 items
        case SettingsCategory::UI: return 2;              // Back, Brightness
        default: return 4;
    }
}

void ui::UiController::settingsBack_() noexcept
{
    // If there are pending changes, confirm before leaving Settings.
    if (settings_dirty_ && settings_popup_mode_ == SettingsPopupMode::None) {
        settings_popup_mode_ = SettingsPopupMode::SaveConfirm;
        settings_popup_selection_ = 0; // default SEND
        dirty_ = true;
        return;
    }

    // Discard changes, return to landing.
    if (settings_ != nullptr) {
        // Restore previewed brightness.
        M5.Display.setBrightness(settings_->ui.brightness);
    }
    in_settings_edit_ = false;
    settings_dirty_ = false;
    settings_value_editing_ = false;
    settings_category_ = SettingsCategory::Main;
    settings_index_ = 0;

    settings_popup_mode_ = SettingsPopupMode::None;
    settings_popup_selection_ = 0;

    settings_value_editor_active_ = false;
    settings_editor_type_ = SettingsEditorValueType::None;
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

    // Push config to test unit (only meaningful while connected).
    if (conn_status_ == ConnStatus::Connected) {
        const auto payload = fatigue_proto::BuildConfigPayload(*settings_);
        (void)espnow::SendConfigSet(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_, &payload, sizeof(payload));
        logf_(now_ms, "TX: ConfigSet dev=%u", fatigue_proto::DEVICE_ID_FATIGUE_TESTER_);
    } else {
        logf_(now_ms, "TX: ConfigSet skipped (not connected)");
    }

    in_settings_edit_ = false;
    settings_dirty_ = false;
    settings_value_editing_ = false;
    settings_popup_mode_ = SettingsPopupMode::None;
    settings_value_editor_active_ = false;
    settings_editor_type_ = SettingsEditorValueType::None;
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

bool ui::UiController::settingsEditorHasChange_() const noexcept
{
    switch (settings_editor_type_) {
        case SettingsEditorValueType::U32: return settings_editor_u32_new_ != settings_editor_u32_old_;
        case SettingsEditorValueType::F32: return settings_editor_f32_new_ != settings_editor_f32_old_;
        case SettingsEditorValueType::Bool: return settings_editor_bool_new_ != settings_editor_bool_old_;
        case SettingsEditorValueType::U8: return settings_editor_u8_new_ != settings_editor_u8_old_;
        case SettingsEditorValueType::I8: return settings_editor_i8_new_ != settings_editor_i8_old_;
        default: return false;
    }
}

void ui::UiController::discardSettingsEditorValue_() noexcept
{
    switch (settings_editor_type_) {
        case SettingsEditorValueType::U32: settings_editor_u32_new_ = settings_editor_u32_old_; break;
        case SettingsEditorValueType::F32: settings_editor_f32_new_ = settings_editor_f32_old_; break;
        case SettingsEditorValueType::Bool: settings_editor_bool_new_ = settings_editor_bool_old_; break;
        case SettingsEditorValueType::U8: settings_editor_u8_new_ = settings_editor_u8_old_; break;
        case SettingsEditorValueType::I8: settings_editor_i8_new_ = settings_editor_i8_old_; break;
        default: break;
    }
}

void ui::UiController::applySettingsEditorValue_() noexcept
{
    // Apply the editor's "new" value back into edit_settings_ for the active target.
    switch (settings_editor_category_) {
        case SettingsCategory::FatigueTest:
            if (settings_editor_type_ == SettingsEditorValueType::U32) {
                if (settings_editor_index_ == 1) {
                    edit_settings_.test_unit.cycle_amount = settings_editor_u32_new_;
                    settings_dirty_ = true;
                } else if (settings_editor_index_ == 4) {
                    // Dwell time (edited in 0.1s increments, stored as ms) - index 4 in new layout
                    edit_settings_.test_unit.dwell_time_ms = settings_editor_u32_new_ * 100u;
                    settings_dirty_ = true;
                }
            } else if (settings_editor_type_ == SettingsEditorValueType::F32) {
                if (settings_editor_index_ == 2) {
                    // VMAX (RPM) - index 2 in new layout
                    edit_settings_.test_unit.oscillation_vmax_rpm = std::max(5.0f, settings_editor_f32_new_);
                    settings_dirty_ = true;
                } else if (settings_editor_index_ == 3) {
                    // AMAX (rev/s²) - index 3 in new layout
                    edit_settings_.test_unit.oscillation_amax_rev_s2 = std::max(0.5f, settings_editor_f32_new_);
                    settings_dirty_ = true;
                }
            }
            break;

        case SettingsCategory::BoundsFinding:
            if (settings_editor_index_ == 1 && settings_editor_type_ == SettingsEditorValueType::Bool) {
                edit_settings_.test_unit.bounds_method_stallguard = settings_editor_bool_new_;
                settings_dirty_ = true;
            } else if (settings_editor_index_ == 4 && settings_editor_type_ == SettingsEditorValueType::I8) {
                edit_settings_.test_unit.stallguard_sgt = settings_editor_i8_new_;
                settings_dirty_ = true;
            } else if (settings_editor_type_ == SettingsEditorValueType::F32) {
                if (settings_editor_index_ == 2) {
                    edit_settings_.test_unit.bounds_search_velocity_rpm = std::max(0.0f, settings_editor_f32_new_);
                    settings_dirty_ = true;
                } else if (settings_editor_index_ == 3) {
                    edit_settings_.test_unit.stallguard_min_velocity_rpm = std::max(0.0f, settings_editor_f32_new_);
                    settings_dirty_ = true;
                } else if (settings_editor_index_ == 5) {
                    edit_settings_.test_unit.stall_detection_current_factor = std::max(0.0f, settings_editor_f32_new_);
                    settings_dirty_ = true;
                } else if (settings_editor_index_ == 6) {
                    edit_settings_.test_unit.bounds_search_accel_rev_s2 = std::max(0.0f, settings_editor_f32_new_);
                    settings_dirty_ = true;
                }
            }
            break;

        case SettingsCategory::UI:
            if (settings_editor_index_ == 1 && settings_editor_type_ == SettingsEditorValueType::U8) {
                edit_settings_.ui.brightness = settings_editor_u8_new_;
                // Preview immediately
                M5.Display.setBrightness(edit_settings_.ui.brightness);
                settings_dirty_ = true;
            }
            break;

        case SettingsCategory::Main:
            break;
    }
}

void ui::UiController::beginSettingsValueEditor_() noexcept
{
    settings_value_editor_active_ = true;
    settings_popup_mode_ = SettingsPopupMode::None;
    settings_popup_selection_ = 0;

    settings_editor_category_ = settings_category_;
    settings_editor_index_ = settings_index_;
    settings_editor_type_ = SettingsEditorValueType::None;

    auto round1 = [](float v) -> float {
        return std::round(v * 10.0f) / 10.0f;
    };

    // Snapshot the current value for the selected item.
    // PROTOCOL V2: Menu layout is: 1=Cycles, 2=VMAX, 3=AMAX, 4=Dwell
    switch (settings_category_) {
        case SettingsCategory::FatigueTest:
            if (settings_index_ == 1) {
                // Cycles (U32)
                settings_editor_type_ = SettingsEditorValueType::U32;
                settings_editor_u32_old_ = edit_settings_.test_unit.cycle_amount;
                settings_editor_u32_new_ = settings_editor_u32_old_;
                settings_editor_u32_step_ = 10;  // Start with step of 10
            } else if (settings_index_ == 2) {
                // VMAX (F32 RPM)
                settings_editor_type_ = SettingsEditorValueType::F32;
                settings_editor_f32_old_ = round1(edit_settings_.test_unit.oscillation_vmax_rpm);
                settings_editor_f32_new_ = settings_editor_f32_old_;
                initSettingsEditorStep_();
            } else if (settings_index_ == 3) {
                // AMAX (F32 rev/s²)
                settings_editor_type_ = SettingsEditorValueType::F32;
                settings_editor_f32_old_ = round1(edit_settings_.test_unit.oscillation_amax_rev_s2);
                settings_editor_f32_new_ = settings_editor_f32_old_;
                initSettingsEditorStep_();
            } else if (settings_index_ == 4) {
                // Dwell (edited in 0.1s increments, stored as ms)
                settings_editor_type_ = SettingsEditorValueType::U32;
                // Represent dwell in tenths of a second (0.1s units)
                settings_editor_u32_old_ = (edit_settings_.test_unit.dwell_time_ms + 50u) / 100u;
                settings_editor_u32_new_ = settings_editor_u32_old_;
                settings_editor_u32_step_ = 1;  // Start with 0.1s increments
            }
            break;

        case SettingsCategory::BoundsFinding:
            if (settings_index_ == 1) {
                settings_editor_type_ = SettingsEditorValueType::Bool;
                settings_editor_bool_old_ = edit_settings_.test_unit.bounds_method_stallguard;
                settings_editor_bool_new_ = settings_editor_bool_old_;
            } else if (settings_index_ == 4) {
                settings_editor_type_ = SettingsEditorValueType::I8;
                settings_editor_i8_old_ = edit_settings_.test_unit.stallguard_sgt;
                settings_editor_i8_new_ = settings_editor_i8_old_;
            } else {
                settings_editor_type_ = SettingsEditorValueType::F32;
                if (settings_index_ == 2) {
                    settings_editor_f32_old_ = round1(edit_settings_.test_unit.bounds_search_velocity_rpm);
                } else if (settings_index_ == 3) {
                    settings_editor_f32_old_ = round1(edit_settings_.test_unit.stallguard_min_velocity_rpm);
                } else if (settings_index_ == 5) {
                    settings_editor_f32_old_ = round1(edit_settings_.test_unit.stall_detection_current_factor);
                } else if (settings_index_ == 6) {
                    settings_editor_f32_old_ = round1(edit_settings_.test_unit.bounds_search_accel_rev_s2);
                } else {
                    settings_editor_f32_old_ = 0.0f;
                }
                settings_editor_f32_new_ = settings_editor_f32_old_;
                initSettingsEditorStep_();
            }
            break;

        case SettingsCategory::UI:
            if (settings_index_ == 1) {
                settings_editor_type_ = SettingsEditorValueType::U8;
                settings_editor_u8_old_ = edit_settings_.ui.brightness;
                settings_editor_u8_new_ = settings_editor_u8_old_;
            }
            break;

        case SettingsCategory::Main:
            // Should not happen (main items aren't editable), but keep it safe.
            settings_editor_type_ = SettingsEditorValueType::None;
            break;
    }
}

void ui::UiController::handleSettingsValueEdit_(int delta) noexcept
{
    if (!settings_value_editor_active_ || delta == 0) {
        return;
    }

    playBeep_(delta > 0 ? 1 : 0);

    auto clamp_add_u32 = [](uint32_t value, int delta_steps, uint32_t step) -> uint32_t {
        const int64_t next = static_cast<int64_t>(value) + static_cast<int64_t>(delta_steps) * static_cast<int64_t>(step);
        if (next < 0) return 0;
        if (next > static_cast<int64_t>(UINT32_MAX)) return UINT32_MAX;
        return static_cast<uint32_t>(next);
    };

    switch (settings_editor_type_) {
        case SettingsEditorValueType::U32: {
            settings_editor_u32_new_ = clamp_add_u32(settings_editor_u32_new_, delta, settings_editor_u32_step_);
            break;
        }
        case SettingsEditorValueType::F32: {
            const float step = std::max(0.0001f, settings_editor_f32_step_);
            const float next = std::max(0.0f, settings_editor_f32_new_ + step * static_cast<float>(delta));
            settings_editor_f32_new_ = std::round(next * 10.0f) / 10.0f;
            break;
        }
        case SettingsEditorValueType::Bool:
            settings_editor_bool_new_ = !settings_editor_bool_new_;
            break;
        case SettingsEditorValueType::U8: {
            const int next = static_cast<int>(settings_editor_u8_new_) + delta * 5;
            settings_editor_u8_new_ = static_cast<uint8_t>(std::clamp(next, 10, 255));
            // Preview brightness immediately.
            if (settings_editor_category_ == SettingsCategory::UI && settings_editor_index_ == 1) {
                M5.Display.setBrightness(settings_editor_u8_new_);
            }
            break;
        }
        case SettingsEditorValueType::I8: {
            // SGT: allow [-64, 63] plus 127="Default".
            auto next_sgt = [](int8_t current, int dir) -> int8_t {
                if (dir == 0) return current;

                if (current == 127) {
                    return (dir > 0) ? static_cast<int8_t>(-64) : static_cast<int8_t>(63);
                }

                const int next = static_cast<int>(current) + dir;
                if (next > 63) return 127;
                if (next < -64) return 127;
                return static_cast<int8_t>(next);
            };

            settings_editor_i8_new_ = next_sgt(settings_editor_i8_new_, (delta > 0) ? 1 : -1);
            break;
        }
        default:
            break;
    }
}

void ui::UiController::getSettingsEditorF32StepOptions_(const float*& steps, size_t& count) const noexcept
{
    // Uniform float editor steps across all float settings:
    // user wants: 0.1, 1, 10 (no finer than one decimal place).
    static constexpr float kSteps[] = {0.1f, 1.0f, 10.0f};
    steps = kSteps;
    count = sizeof(kSteps) / sizeof(kSteps[0]);
}

void ui::UiController::initSettingsEditorStep_() noexcept
{
    if (settings_editor_type_ != SettingsEditorValueType::F32) {
        return;
    }

    const float* steps = nullptr;
    size_t count = 0;
    getSettingsEditorF32StepOptions_(steps, count);
    if (steps == nullptr || count == 0) {
        settings_editor_f32_step_ = 0.1f;
        return;
    }

    // Choose a sensible default per field (matches the first "preferred" choice in the step array).
    // RPM arrays are {0.1, 1, 10} -> default 1
    // rev/s^2 arrays are {0.01, 0.1, 1} -> default 0.1
    // stall factor arrays are {0.01, 0.05, 0.1, 0.5} -> default 0.05
    if (count >= 2) {
        settings_editor_f32_step_ = steps[1];
    } else {
        settings_editor_f32_step_ = steps[0];
    }
}

void ui::UiController::cycleSettingsEditorStep_() noexcept
{
    // Handle F32 types (VMAX, AMAX, velocity, etc.)
    if (settings_editor_type_ == SettingsEditorValueType::F32) {
        const float* steps = nullptr;
        size_t count = 0;
        getSettingsEditorF32StepOptions_(steps, count);
        if (steps == nullptr || count == 0) {
            return;
        }

        // Find current step in the option list, then advance.
        size_t idx = 0;
        const float cur = settings_editor_f32_step_;
        bool found = false;
        for (size_t i = 0; i < count; ++i) {
            if (std::fabs(static_cast<double>(steps[i] - cur)) < 1e-6) {
                idx = i;
                found = true;
                break;
            }
        }
        if (!found) {
            idx = 0;
        }
        idx = (idx + 1) % count;
        settings_editor_f32_step_ = steps[idx];
        return;
    }
    
    // Handle U32 types (Cycles, Dwell)
    if (settings_editor_type_ == SettingsEditorValueType::U32) {
        if (settings_editor_category_ == SettingsCategory::FatigueTest) {
            if (settings_editor_index_ == 1) {
                // Cycles: 10, 100, 1000 steps (matches quick settings)
                static constexpr uint32_t kCycleSteps[] = {10, 100, 1000};
                static constexpr size_t kCycleCount = sizeof(kCycleSteps) / sizeof(kCycleSteps[0]);
                
                size_t idx = 0;
                for (size_t i = 0; i < kCycleCount; ++i) {
                    if (kCycleSteps[i] == settings_editor_u32_step_) {
                        idx = i;
                        break;
                    }
                }
                idx = (idx + 1) % kCycleCount;
                settings_editor_u32_step_ = kCycleSteps[idx];
            } else if (settings_editor_index_ == 4) {
                // Dwell: 1, 10, 100 steps (in 0.1s units, so 0.1s, 1s, 10s increments)
                static constexpr uint32_t kDwellSteps[] = {1, 10, 100};
                static constexpr size_t kDwellCount = sizeof(kDwellSteps) / sizeof(kDwellSteps[0]);
                size_t idx = 0;
                for (size_t i = 0; i < kDwellCount; ++i) {
                    if (kDwellSteps[i] == settings_editor_u32_step_) {
                        idx = i;
                        break;
                    }
                }
                idx = (idx + 1) % kDwellCount;
                settings_editor_u32_step_ = kDwellSteps[idx];
            }
        }
    }
}

void ui::UiController::handleSettingsPopupInput_(int delta, bool click, uint32_t now_ms) noexcept
{
    if (settings_popup_mode_ == SettingsPopupMode::None) {
        return;
    }

    const int max_sel = (settings_popup_mode_ == SettingsPopupMode::ValueChangeConfirm || settings_popup_mode_ == SettingsPopupMode::SaveConfirm) ? 1 : 2;

    if (delta != 0) {
        const int next = static_cast<int>(settings_popup_selection_) + (delta > 0 ? 1 : -1);
        settings_popup_selection_ = static_cast<uint8_t>(std::clamp(next, 0, max_sel));
        playBeep_(delta > 0 ? 1 : 0);
        dirty_ = true;
    }

    if (!click) {
        return;
    }

    playBeep_(2);

    if (settings_popup_mode_ == SettingsPopupMode::ValueChangeConfirm) {
        // 0=KEEP, 1=DISCARD
        if (settings_popup_selection_ == 0) {
            applySettingsEditorValue_();
        } else {
            discardSettingsEditorValue_();
            // Restore previewed brightness if needed.
            if (settings_editor_category_ == SettingsCategory::UI && settings_editor_index_ == 1 && settings_editor_type_ == SettingsEditorValueType::U8) {
                M5.Display.setBrightness(settings_editor_u8_old_);
            }
        }

        settings_popup_mode_ = SettingsPopupMode::None;
        settings_popup_selection_ = 0;
        settings_value_editor_active_ = false;
        settings_editor_type_ = SettingsEditorValueType::None;
        dirty_ = true;
        return;
    }

    // SaveConfirm: leaving Settings with unsent changes.
    // 0=SEND, 1=RESYNC (discard local edits and show machine config)
    if (settings_popup_mode_ == SettingsPopupMode::SaveConfirm) {
        if (settings_popup_selection_ == 0) {
            if (conn_status_ != ConnStatus::Connected) {
                playBeep_(1);
                logf_(now_ms, "UI: not connected - cannot send changes");
                dirty_ = true;
                return;
            }
            settingsSave_(now_ms);
            return;
        }

        // RESYNC: discard edits and return to landing.
        if (settings_ != nullptr) {
            edit_settings_ = *settings_;
            original_settings_ = edit_settings_;
            M5.Display.setBrightness(settings_->ui.brightness);
        }

        in_settings_edit_ = false;
        settings_dirty_ = false;
        settings_value_editing_ = false;
        settings_category_ = SettingsCategory::Main;
        settings_index_ = 0;
        settings_popup_mode_ = SettingsPopupMode::None;
        settings_popup_selection_ = 0;
        settings_value_editor_active_ = false;
        settings_editor_type_ = SettingsEditorValueType::None;
        page_ = Page::Landing;
        dirty_ = true;
        return;
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
    // Modern button with gradient-like effect
    const int16_t r = rect.h / 3;  // Rounded corners proportional to height
    
    uint16_t bg_color = colors::bg_elevated;
    uint16_t border_color = colors::button_border;
    uint16_t text_color = colors::text_secondary;
    
    if (pressed) {
        bg_color = colors::accent_blue;
        border_color = colors::accent_cyan;
        text_color = colors::text_primary;
    } else if (focused) {
        bg_color = colors::button_active;
        border_color = colors::accent_blue;
        text_color = colors::text_primary;
    }
    
    // Draw filled button with smooth corners
    canvas_->fillSmoothRoundRect(rect.x, rect.y, rect.w, rect.h, r, bg_color);
    
    // Draw border (2px for focused)
    canvas_->drawRoundRect(rect.x, rect.y, rect.w, rect.h, r, border_color);
    if (focused) {
        canvas_->drawRoundRect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, r - 1, border_color);
    }
    
    // Draw label centered with size 2 font
    canvas_->setTextColor(text_color);
    canvas_->setTextSize(2);
    const int16_t tw = static_cast<int16_t>(canvas_->textWidth(label));
    const int16_t th = 14;  // Approx height for size 2
    canvas_->setCursor(rect.x + (rect.w - tw) / 2, rect.y + (rect.h - th) / 2);
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

void ui::UiController::drawActionButton_(int16_t x, int16_t y, int16_t w, int16_t h,
                                         const char* label, bool selected,
                                         uint16_t accent_color, bool dark_text) noexcept
{
    // Beautiful action button with accent color
    const int16_t r = h / 3;
    
    // Dim version of accent for unselected state
    const uint16_t dim_color = ((accent_color >> 1) & 0x7BEF);  // Halve RGB components
    
    uint16_t bg = selected ? accent_color : dim_color;
    uint16_t border = accent_color;
    uint16_t text_color = dark_text ? colors::bg_primary : colors::text_primary;
    
    // Draw filled button
    canvas_->fillSmoothRoundRect(x, y, w, h, r, bg);
    
    // Draw border (thicker when selected)
    canvas_->drawRoundRect(x, y, w, h, r, border);
    if (selected) {
        canvas_->drawRoundRect(x + 1, y + 1, w - 2, h - 2, r - 1, border);
    }
    
    // Draw label centered with size 2 font
    canvas_->setTextColor(text_color);
    canvas_->setTextSize(2);
    const int16_t tw = static_cast<int16_t>(canvas_->textWidth(label));
    const int16_t th = 14;
    canvas_->setCursor(x + (w - tw) / 2, y + (h - th) / 2);
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
        
        // Draw hollow ring highlight FIRST (behind icon) for selected item only
        if (is_selected && !animating) {
            // Draw ring outside the icon area (icon radius is 21, draw at 23-26)
            const int16_t base_r = menu_config_.icon_bg_radius;  // 22
            canvas_->drawCircle(ix, iy, base_r + 2, item.color);
            canvas_->drawCircle(ix, iy, base_r + 3, item.color);
            canvas_->drawCircle(ix, iy, base_r + 4, item.color);
        }
        
        // Draw icon centered ON TOP (icon already has colored background baked in)
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

    // Connection status pill (centered, below the Settings menu icon).
    {
        const char* conn_text = "DISCONNECTED";
        uint16_t conn_color = colors::accent_red;
        switch (conn_status_) {
            case ConnStatus::Connected:
                conn_text = "CONNECTED";
                conn_color = colors::accent_green;
                break;
            case ConnStatus::Connecting:
                conn_text = "CONNECTING";
                conn_color = colors::accent_yellow;
                break;
            case ConnStatus::Disconnected:
            default:
                break;
        }

        constexpr int16_t kPillH = 16;
        constexpr int16_t kPadX = 8;
        constexpr int16_t kRadius = 8;
        // Extra spacing so the animated selector dot doesn't pass over the pill
        // when the carousel is near the Settings icon.
        constexpr int16_t kGap = 14;

        constexpr int kSettingsIndex = 0; // kMenuItems_[0] = Settings
        const Point2D settings_pos = menu_selector_.getIconPosition(kSettingsIndex);
        const int16_t settings_y = static_cast<int16_t>(settings_pos.y);
        const int16_t pill_center_y = static_cast<int16_t>(settings_y + menu_config_.icon_bg_radius + kGap + (kPillH / 2));

        canvas_->setTextSize(1);
        const int16_t tw = static_cast<int16_t>(canvas_->textWidth(conn_text));
        const int16_t pill_w = static_cast<int16_t>(tw + (kPadX * 2));
        int16_t pill_x = static_cast<int16_t>(cx - (pill_w / 2));
        int16_t pill_y = static_cast<int16_t>(pill_center_y - (kPillH / 2));
        if (pill_x < 4) pill_x = 4;
        if ((pill_x + pill_w) > 236) pill_x = static_cast<int16_t>(236 - pill_w);
        if (pill_y < 4) pill_y = 4;

        canvas_->fillRoundRect(pill_x, pill_y, pill_w, kPillH, kRadius, colors::bg_card);
        canvas_->drawRoundRect(pill_x, pill_y, pill_w, kPillH, kRadius, conn_color);
        canvas_->setTextColor(conn_color);
        canvas_->setCursor(static_cast<int16_t>(pill_x + kPadX), static_cast<int16_t>(pill_y + 4));
        canvas_->print(conn_text);
    }
    
    // Draw connection indicator
    drawConnectionIndicator_(now_ms);
    
    // Draw selector dot (animated)
    drawCircularMenuSelector_(now_ms);
    
    // Draw icons in circular arrangement
    drawCircularMenuIcons_(now_ms);
    
    // Draw tag/label in center
    drawCircularMenuTag_(now_ms);
    
    // Status hint (below center tag): keep it away from carousel icons.
    // The icon ring radius is ~95px; a bottom-edge label can overlap whichever
    // icon is currently at the bottom (often interpreted as being “on” that icon).
    const int16_t status_center_y = static_cast<int16_t>(cy + 56);
    constexpr int16_t kPillH = 18;
    constexpr int16_t kPillPadX = 10;
    constexpr int16_t kPillTextY = 5;
    constexpr uint16_t kPillFill = 0x2104; // subtle dark fill (matches outer ring)

    auto draw_pill = [&](const char* text, uint16_t border_color, uint16_t text_color) {
        canvas_->setTextSize(1);
        const int16_t tw = static_cast<int16_t>(canvas_->textWidth(text));
        const int16_t pill_w = static_cast<int16_t>(tw + (kPillPadX * 2));
        const int16_t pill_x = static_cast<int16_t>(cx - (pill_w / 2));
        const int16_t pill_y = static_cast<int16_t>(status_center_y - (kPillH / 2));
        canvas_->fillRoundRect(pill_x, pill_y, pill_w, kPillH, 9, kPillFill);
        canvas_->drawRoundRect(pill_x, pill_y, pill_w, kPillH, 9, border_color);
        canvas_->setTextColor(text_color);
        canvas_->setCursor(static_cast<int16_t>(pill_x + kPillPadX), static_cast<int16_t>(pill_y + kPillTextY));
        canvas_->print(text);
    };

    // Only show status if connected - don't show stale "Idle" or "Running" when disconnected
    if (conn_status_ == ConnStatus::Connected && have_status_) {
        const char* state_str = "IDLE";
        uint16_t state_color = colors::state_idle;
        switch (static_cast<fatigue_proto::TestState>(last_status_.state)) {
            case fatigue_proto::TestState::Running:
                state_str = "RUNNING";
                state_color = colors::state_running;
                break;
            case fatigue_proto::TestState::Paused:
                state_str = "PAUSED";
                state_color = colors::state_paused;
                break;
            case fatigue_proto::TestState::Error:
                state_str = "ERROR";
                state_color = colors::state_error;
                break;
            default:
                break;
        }

        char status_buf[32];
        snprintf(status_buf, sizeof(status_buf), "%s #%" PRIu32, state_str, last_status_.cycle_number);
        draw_pill(status_buf, state_color, state_color);
    } else if (conn_status_ == ConnStatus::Connecting) {
        // Animated connecting indicator (dots)
        const int dot_count = ((now_ms / 500) % 4);
        char conn_buf[20] = "WAITING";
        for (int i = 0; i < dot_count; ++i) {
            strncat(conn_buf, ".", sizeof(conn_buf) - strlen(conn_buf) - 1);
        }
        draw_pill(conn_buf, 0x8410, 0xAD55);
    }
    // When disconnected, connection indicator dot (red) is enough - no text needed
}

void ui::UiController::drawSettings_(uint32_t now_ms) noexcept
{
    (void)now_ms;
    
    if (!in_settings_edit_) {
        enterSettings_();
    }

    // Dedicated value editor screen (optionally with confirm popup overlay).
    if (settings_value_editor_active_) {
        drawSettingsValueEditor_(now_ms);
        if (settings_popup_mode_ != SettingsPopupMode::None) {
            drawSettingsPopup_(now_ms);
        }
        return;
    }

    // Popup overlay (currently used for value-change confirmation).
    if (settings_popup_mode_ != SettingsPopupMode::None) {
        drawSettingsPopup_(now_ms);
        return;
    }

    // === SMOOTH ANIMATION ===
    const float anim_speed = 0.70f;
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
    static const char* main_values[] = {"Return to home", "Motion settings", "Stall detection", "Display options"};
    
    // Fatigue Test labels - PROTOCOL V2: velocity/acceleration control
    static const char* fatigue_labels[] = {"< Back", "Cycles", "VMAX (RPM)", kLabelAmaxRevPerS2Ui, "Dwell (s)"};
    
    // Bounds Finding labels
    static const char* bounds_labels[] = {"< Back", "Mode", "Search Speed", "SG Min Vel", "SGT", "Stall Factor", "Search Accel"};
    
    // UI labels
    static const char* ui_labels[] = {"< Back", "Brightness"};
    
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
            item_count = 5;  // Back, Cycles, VMAX, AMAX, Dwell
            for (int i = 0; i < item_count; ++i) labels[i] = fatigue_labels[i];
            snprintf(values[0], sizeof(values[0]), "Back to settings");
            snprintf(values[1], sizeof(values[1]), "%" PRIu32, edit_settings_.test_unit.cycle_amount);
            snprintf(values[2], sizeof(values[2]), "%.1f", static_cast<double>(edit_settings_.test_unit.oscillation_vmax_rpm));
            snprintf(values[3], sizeof(values[3]), "%.1f", static_cast<double>(edit_settings_.test_unit.oscillation_amax_rev_s2));
            if ((edit_settings_.test_unit.dwell_time_ms % 1000u) == 0u) {
                snprintf(values[4], sizeof(values[4]), "%" PRIu32, (edit_settings_.test_unit.dwell_time_ms / 1000u));
            } else {
                snprintf(values[4], sizeof(values[4]), "%.1f", static_cast<double>(edit_settings_.test_unit.dwell_time_ms) / 1000.0);
            }
            break;
            
        case SettingsCategory::BoundsFinding:
            title = "BOUNDS";
            item_count = 7;
            for (int i = 0; i < item_count; ++i) labels[i] = bounds_labels[i];
            snprintf(values[0], sizeof(values[0]), "Back to settings");
            snprintf(values[1], sizeof(values[1]), "%s", edit_settings_.test_unit.bounds_method_stallguard ? "StallGuard" : "Encoder");
            snprintf(values[2], sizeof(values[2]), "%.1f rpm", static_cast<double>(edit_settings_.test_unit.bounds_search_velocity_rpm));
            snprintf(values[3], sizeof(values[3]), "%.1f rpm", static_cast<double>(edit_settings_.test_unit.stallguard_min_velocity_rpm));
            if (edit_settings_.test_unit.stallguard_sgt == 127) {
                snprintf(values[4], sizeof(values[4]), "%s", "Default");
            } else {
                snprintf(values[4], sizeof(values[4]), "%d", static_cast<int>(edit_settings_.test_unit.stallguard_sgt));
            }
            snprintf(values[5], sizeof(values[5]), "%.1fx", static_cast<double>(edit_settings_.test_unit.stall_detection_current_factor));
            snprintf(values[6], sizeof(values[6]), "%.1f %s", static_cast<double>(edit_settings_.test_unit.bounds_search_accel_rev_s2), kUnitRevPerS2Ui);
            break;
            
        case SettingsCategory::UI:
            title = "UI SETTINGS";
            item_count = 2;
            for (int i = 0; i < item_count; ++i) labels[i] = ui_labels[i];
            snprintf(values[0], sizeof(values[0]), "Back to settings");
            snprintf(values[1], sizeof(values[1]), "%d%%", static_cast<int>(edit_settings_.ui.brightness * 100 / 255));
            break;
    }
    
    // === DRAW MENU ITEMS (all drawing to canvas_ for flicker-free) ===
    constexpr int16_t header_h = 54;
    for (int i = 0; i < item_count; ++i) {
        const float item_y_offset = (static_cast<float>(i) * kSettingsItemHeight_) - settings_anim_offset_;
        const int16_t item_y = menu_center_y + static_cast<int16_t>(item_y_offset);

        if (item_y < (header_h + 4) || item_y > 192) continue;
        
        const bool selected = (settings_index_ == i);
        const bool is_category = (settings_category_ == SettingsCategory::Main && i > 0);
        const bool editing = false; // No inline editing; value changes happen in a dedicated editor screen.
        
        // Draw item card - inline to use canvas_
        const int16_t card_x = 25;
        const int16_t card_w = 190;
        const int16_t card_h = 40;
        
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
        
        // Label (larger)
        canvas_->setTextSize(2);
        canvas_->setTextColor(selected ? TFT_WHITE : colors::text_primary);
        canvas_->setCursor(card_x + 10, item_y - 14);
        canvas_->print(labels[i]);
        
        // Value: prefer larger for readability, but never allow it to overflow the card.
        canvas_->setTextColor(selected ? colors::accent_yellow : colors::text_secondary);
        canvas_->setTextSize(2);
        int16_t vw = static_cast<int16_t>(canvas_->textWidth(values[i]));
        if (vw > (card_w - 20)) {
            canvas_->setTextSize(1);
        }
        canvas_->setCursor(card_x + 10, item_y + 4);
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
    canvas_->fillRect(0, 0, 240, header_h, lgfx::color565(15, 15, 20));
    canvas_->setTextColor(0xFA7000);
    {
        // Fit title to circular safe area at y~26 (avoid clipping on round display)
        const float r = 118.0f;
        const float cy_safe = 120.0f;
        const float dy = 26.0f - cy_safe;
        const float half = std::sqrt(std::max(0.0f, r * r - dy * dy));
        const int16_t max_w = static_cast<int16_t>(std::max(0.0f, (half * 2.0f) - 28.0f));

        canvas_->setTextSize(2);
        int16_t title_w = static_cast<int16_t>(canvas_->textWidth(title));
        if (title_w > max_w) {
            canvas_->setTextSize(1);
            title_w = static_cast<int16_t>(canvas_->textWidth(title));
        }
        canvas_->setCursor(cx - title_w / 2, 12);
        canvas_->print(title);
    }
    
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
        const float r = 118.0f;
        const float cy_safe = 120.0f;
        const int16_t crumb_y = 34;
        const float dy = (static_cast<float>(crumb_y) + 4.0f) - cy_safe; // ~text mid
        const float half = std::sqrt(std::max(0.0f, r * r - dy * dy));
        const int16_t max_w = static_cast<int16_t>(std::max(0.0f, (half * 2.0f) - 18.0f));

        const char* crumb = "Settings >";
        int16_t w = static_cast<int16_t>(canvas_->textWidth(crumb));
        if (w > max_w) {
            crumb = "Settings";
            w = static_cast<int16_t>(canvas_->textWidth(crumb));
        }

        canvas_->setCursor(cx - w / 2, crumb_y);
        canvas_->print(crumb);
    }
}

void ui::UiController::drawSettingsValueEditor_(uint32_t now_ms) noexcept
{
    (void)now_ms;

    const int16_t cx = 120;
    const int16_t cy = 120;

    // Background
    canvas_->fillScreen(colors::bg_primary);
    canvas_->drawCircle(cx, cy, 118, colors::bg_card);

    // Resolve label + formatting for the current editor target
    const char* label = "";
    const char* unit = "";
    bool bool_is_mode = false;
    bool unit_is_rev_per_s2 = false;

    switch (settings_editor_category_) {
        case SettingsCategory::FatigueTest:
            if (settings_editor_index_ == 1) { label = "Cycles"; }
            else if (settings_editor_index_ == 2) { label = "VMAX"; unit = "rpm"; }
            else if (settings_editor_index_ == 3) { label = "AMAX"; unit = kUnitRevPerS2Ui; unit_is_rev_per_s2 = true; }
            else if (settings_editor_index_ == 4) { label = "Dwell"; unit = "s"; }
            break;
        case SettingsCategory::BoundsFinding:
            if (settings_editor_index_ == 1) { label = "Mode"; bool_is_mode = true; }
            else if (settings_editor_index_ == 2) { label = "Search Speed"; unit = "rpm"; }
            else if (settings_editor_index_ == 3) { label = "SG Min Vel"; unit = "rpm"; }
            else if (settings_editor_index_ == 4) { label = "SGT"; }
            else if (settings_editor_index_ == 5) { label = "Stall Factor"; unit = "x"; }
            else if (settings_editor_index_ == 6) { label = "Search Accel"; unit = kUnitRevPerS2Ui; unit_is_rev_per_s2 = true; }
            break;
        case SettingsCategory::UI:
            if (settings_editor_index_ == 1) { label = "Brightness"; unit = "%"; }
            break;
        case SettingsCategory::Main:
            break;
    }

    // Header bar
    canvas_->fillRect(0, 0, 240, 44, colors::bg_elevated);
    canvas_->setTextColor(colors::accent_orange);

    // Fit the title to the circular top area (shrink, then split if needed)
    const float r = 118.0f;
    auto maxWidthAtY = [&](int16_t y_mid, int16_t margin) -> int16_t {
        const float dy = static_cast<float>(y_mid) - static_cast<float>(cy);
        const float half = std::sqrt(std::max(0.0f, r * r - dy * dy));
        return static_cast<int16_t>(std::max(0.0f, (half * 2.0f) - static_cast<float>(margin)));
    };

    const int16_t max_w_size2 = maxWidthAtY(22, 18);
    canvas_->setTextSize(2);
    int16_t lw = static_cast<int16_t>(canvas_->textWidth(label));

    if (lw <= max_w_size2) {
        canvas_->setCursor(cx - lw / 2, 14);
        canvas_->print(label);
    } else {
        // Try smaller text
        const int16_t max_w_size1 = maxWidthAtY(24, 18);
        canvas_->setTextSize(1);
        lw = static_cast<int16_t>(canvas_->textWidth(label));

        if (lw <= max_w_size1) {
            canvas_->setCursor(cx - lw / 2, 18);
            canvas_->print(label);
        } else {
            // Split on '/' first, then on space (best effort)
            char line1[20] = {0};
            char line2[20] = {0};
            const char* split = strchr(label, '/');
            if (split == nullptr) {
                split = strrchr(label, ' ');
            }

            if (split != nullptr) {
                const size_t n1 = std::min(sizeof(line1) - 1, static_cast<size_t>(split - label));
                memcpy(line1, label, n1);
                line1[n1] = '\0';

                const char* rest = (*split == '/') ? (split + 1) : (split + 1);
                while (*rest == ' ') { ++rest; }
                snprintf(line2, sizeof(line2), "%s", rest);
            } else {
                snprintf(line1, sizeof(line1), "%s", label);
            }

            canvas_->setTextSize(1);
            canvas_->setTextColor(colors::accent_orange);

            if (line2[0] == '\0') {
                const int16_t w1 = static_cast<int16_t>(canvas_->textWidth(line1));
                canvas_->setCursor(cx - w1 / 2, 18);
                canvas_->print(line1);
            } else {
                const int16_t w1 = static_cast<int16_t>(canvas_->textWidth(line1));
                const int16_t w2 = static_cast<int16_t>(canvas_->textWidth(line2));
                canvas_->setCursor(cx - w1 / 2, 12);
                canvas_->print(line1);
                canvas_->setCursor(cx - w2 / 2, 26);
                canvas_->print(line2);
            }
        }
    }

    // Old value line
    char old_buf[40] = {0};
    char new_buf[40] = {0};
    char new_value_only[32] = {0};
    const bool has_unit = (unit[0] != '\0');
    const bool render_unit_separately = has_unit && (unit_is_rev_per_s2 || strlen(unit) > 4);
    switch (settings_editor_type_) {
        case SettingsEditorValueType::U32:
            if (settings_editor_category_ == SettingsCategory::FatigueTest && settings_editor_index_ == 4) {
                // Dwell: editor stores tenths of a second; display as seconds with 0.1s resolution.
                const double old_s = static_cast<double>(settings_editor_u32_old_) * 0.1;
                const double new_s = static_cast<double>(settings_editor_u32_new_) * 0.1;
                snprintf(old_buf, sizeof(old_buf), "Old: %.1f %s", old_s, unit);
                snprintf(new_buf, sizeof(new_buf), "%.1f %s", new_s, unit);
            } else if (has_unit) {
                snprintf(old_buf, sizeof(old_buf), "Old: %" PRIu32 " %s", settings_editor_u32_old_, unit);
                if (render_unit_separately) {
                    snprintf(new_value_only, sizeof(new_value_only), "%" PRIu32, settings_editor_u32_new_);
                } else {
                    snprintf(new_buf, sizeof(new_buf), "%" PRIu32 " %s", settings_editor_u32_new_, unit);
                }
            } else {
                snprintf(old_buf, sizeof(old_buf), "Old: %" PRIu32, settings_editor_u32_old_);
                snprintf(new_buf, sizeof(new_buf), "%" PRIu32, settings_editor_u32_new_);
            }
            break;
        case SettingsEditorValueType::F32:
            if (has_unit) {
                snprintf(old_buf, sizeof(old_buf), "Old: %.1f %s", static_cast<double>(settings_editor_f32_old_), unit);
                if (render_unit_separately) {
                    snprintf(new_value_only, sizeof(new_value_only), "%.1f", static_cast<double>(settings_editor_f32_new_));
                } else {
                    snprintf(new_buf, sizeof(new_buf), "%.1f %s", static_cast<double>(settings_editor_f32_new_), unit);
                }
            } else {
                snprintf(old_buf, sizeof(old_buf), "Old: %.1f", static_cast<double>(settings_editor_f32_old_));
                snprintf(new_buf, sizeof(new_buf), "%.1f", static_cast<double>(settings_editor_f32_new_));
            }
            break;
        case SettingsEditorValueType::I8: {
            auto fmt = [](char* buf, size_t n, const char* prefix, int8_t v) {
                if (v == 127) {
                    snprintf(buf, n, "%sDefault", prefix);
                } else {
                    snprintf(buf, n, "%s%d", prefix, static_cast<int>(v));
                }
            };
            fmt(old_buf, sizeof(old_buf), "Old: ", settings_editor_i8_old_);
            fmt(new_buf, sizeof(new_buf), "", settings_editor_i8_new_);
            break;
        }
        case SettingsEditorValueType::Bool:
            if (bool_is_mode) {
                snprintf(old_buf, sizeof(old_buf), "Old: %s", settings_editor_bool_old_ ? "StallGuard" : "Encoder");
                snprintf(new_buf, sizeof(new_buf), "%s", settings_editor_bool_new_ ? "StallGuard" : "Encoder");
            } else {
                snprintf(old_buf, sizeof(old_buf), "Old: %s", settings_editor_bool_old_ ? "Yes" : "No");
                snprintf(new_buf, sizeof(new_buf), "%s", settings_editor_bool_new_ ? "Yes" : "No");
            }
            break;
        case SettingsEditorValueType::U8: {
            const int old_pct = static_cast<int>(settings_editor_u8_old_) * 100 / 255;
            const int new_pct = static_cast<int>(settings_editor_u8_new_) * 100 / 255;
            snprintf(old_buf, sizeof(old_buf), "Old: %d%%", old_pct);
            snprintf(new_buf, sizeof(new_buf), "%d%%", new_pct);
            break;
        }
        default:
            snprintf(old_buf, sizeof(old_buf), "Old: -");
            snprintf(new_buf, sizeof(new_buf), "-");
            break;
    }

    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_hint);
    const int16_t ow = static_cast<int16_t>(canvas_->textWidth(old_buf));
    canvas_->setCursor(cx - ow / 2, 54);
    canvas_->print(old_buf);

    // Big value
    canvas_->setTextSize(4);
    canvas_->setTextColor(colors::text_primary);
    if (render_unit_separately && (new_value_only[0] != '\0')) {
        const int16_t vw = static_cast<int16_t>(canvas_->textWidth(new_value_only));
        canvas_->setCursor(cx - vw / 2, cy - 28);
        canvas_->print(new_value_only);

        // Unit line (smaller), avoids making "rev/s^2" huge.
        // For rev/s^2 we draw a manual superscript to avoid missing glyphs rendering as '|'.
        if (unit_is_rev_per_s2) {
            const int16_t unit_y = static_cast<int16_t>(cy + 10);
            canvas_->setTextColor(colors::text_hint);

            canvas_->setTextSize(2);
            const char* base = "rev/s";
            const int16_t base_w = static_cast<int16_t>(canvas_->textWidth(base));
            canvas_->setTextSize(1);
            const int16_t exp_w = static_cast<int16_t>(canvas_->textWidth("2"));
            const int16_t total_w = static_cast<int16_t>(base_w + exp_w);
            const int16_t x0 = static_cast<int16_t>(cx - total_w / 2);

            canvas_->setTextSize(2);
            canvas_->setCursor(x0, unit_y);
            canvas_->print(base);
            canvas_->setTextSize(1);
            canvas_->setCursor(static_cast<int16_t>(x0 + base_w), static_cast<int16_t>(unit_y - 4));
            canvas_->print("2");
        } else {
            drawCenteredText_(cx, cy + 10, unit, colors::text_hint, 2);
        }
    } else {
        const int16_t vw = static_cast<int16_t>(canvas_->textWidth(new_buf));
        canvas_->setCursor(cx - vw / 2, cy - 22);
        canvas_->print(new_buf);
    }

    // Instructions - styled pills at bottom (like quick settings)
    canvas_->setTextSize(1);

    // --- Step pill ---
    // Show step info for F32 and U32 (FatigueTest Cycles/Dwell) types
    const bool show_step = (settings_editor_type_ == SettingsEditorValueType::F32) ||
        (settings_editor_type_ == SettingsEditorValueType::U32 && 
         settings_editor_category_ == SettingsCategory::FatigueTest &&
         (settings_editor_index_ == 1 || settings_editor_index_ == 4));

    char step_hint_buf[48] = {0};
    if (show_step) {
        if (settings_editor_type_ == SettingsEditorValueType::F32) {
            const float s = settings_editor_f32_step_;
            if (s >= 1.0f) {
                snprintf(step_hint_buf, sizeof(step_hint_buf), "Step:%.0f | Hold:step", static_cast<double>(s));
            } else if (s >= 0.1f) {
                snprintf(step_hint_buf, sizeof(step_hint_buf), "Step:%.1f | Hold:step", static_cast<double>(s));
            } else {
                snprintf(step_hint_buf, sizeof(step_hint_buf), "Step:%.2f | Hold:step", static_cast<double>(s));
            }
        } else if (settings_editor_type_ == SettingsEditorValueType::U32) {
            if (settings_editor_index_ == 4) {
                // Dwell: step is in 0.1s units, display as seconds
                const double step_s = static_cast<double>(settings_editor_u32_step_) * 0.1;
                if (step_s >= 1.0) {
                    snprintf(step_hint_buf, sizeof(step_hint_buf), "Step:%.0fs | Hold:step", step_s);
                } else {
                    snprintf(step_hint_buf, sizeof(step_hint_buf), "Step:%.1fs | Hold:step", step_s);
                }
            } else {
                // Cycles
                snprintf(step_hint_buf, sizeof(step_hint_buf), "Step:%" PRIu32 " | Hold:step", settings_editor_u32_step_);
            }
        }
    } else {
        snprintf(step_hint_buf, sizeof(step_hint_buf), "Rotate to change");
    }

    // --- Action pill ---
    const char* action_hint = "Rotate: change | Click: exit";
    const int16_t action_hw = static_cast<int16_t>(canvas_->textWidth(action_hint));
    const int16_t action_pill_w = action_hw + 16;
    const int16_t action_pill_h = 18;
    const int16_t action_pill_x = cx - action_pill_w / 2;
    const int16_t action_pill_y = 205 - action_pill_h - 6;
    canvas_->fillSmoothRoundRect(action_pill_x, action_pill_y, action_pill_w, action_pill_h, 9, colors::bg_elevated);
    canvas_->drawRoundRect(action_pill_x, action_pill_y, action_pill_w, action_pill_h, 9, colors::text_hint);
    canvas_->setTextColor(colors::text_secondary);
    canvas_->setCursor(cx - action_hw / 2, action_pill_y + 4);
    canvas_->print(action_hint);

    // --- Step pill (bottom) ---
    const int16_t step_hw = static_cast<int16_t>(canvas_->textWidth(step_hint_buf));
    const int16_t step_pill_w = step_hw + 16;
    const int16_t step_pill_h = 18;
    const int16_t step_pill_x = cx - step_pill_w / 2;
    const int16_t step_pill_y = 205;
    canvas_->fillSmoothRoundRect(step_pill_x, step_pill_y, step_pill_w, step_pill_h, 9, colors::bg_elevated);
    canvas_->drawRoundRect(step_pill_x, step_pill_y, step_pill_w, step_pill_h, 9, colors::text_hint);
    canvas_->setTextColor(colors::text_secondary);
    canvas_->setCursor(cx - step_hw / 2, step_pill_y + 4);
    canvas_->print(step_hint_buf);
}

void ui::UiController::drawSettingsPopup_(uint32_t now_ms) noexcept
{
    (void)now_ms;

    const int16_t cx = 120;
    const int16_t cy = 120;
    const int16_t w = 198;
    const int16_t h = 132;
    const int16_t x = cx - w / 2;
    const int16_t y = cy - h / 2;

    drawRoundedRect_(x, y, w, h, 12, colors::bg_elevated, true);
    drawRoundedRect_(x, y, w, h, 12, colors::accent_blue, false);

    const char* title = (settings_popup_mode_ == SettingsPopupMode::ValueChangeConfirm)
        ? "Keep change?"
        : ((settings_popup_mode_ == SettingsPopupMode::SaveConfirm) ? "Send changes?" : "Settings");
    canvas_->setTextSize(2);
    canvas_->setTextColor(colors::text_primary);
    const int16_t tw = static_cast<int16_t>(canvas_->textWidth(title));
    canvas_->setCursor(cx - tw / 2, y + 14);
    canvas_->print(title);

    if (settings_popup_mode_ == SettingsPopupMode::ValueChangeConfirm) {
        char old_line[44] = {0};
        char new_line[44] = {0};

        // Match the unit/label logic used in the editor screen.
        const char* unit = "";
        bool bool_is_mode = false;
        switch (settings_editor_category_) {
            case SettingsCategory::FatigueTest:
                if (settings_editor_index_ == 2) { unit = "rpm"; }
                else if (settings_editor_index_ == 3) { unit = kUnitRevPerS2Ui; }
                else if (settings_editor_index_ == 4) { unit = "s"; }
                break;
            case SettingsCategory::BoundsFinding:
                if (settings_editor_index_ == 1) { bool_is_mode = true; }
                else if (settings_editor_index_ == 2) { unit = "rpm"; }
                else if (settings_editor_index_ == 3) { unit = "rpm"; }
                else if (settings_editor_index_ == 5) { unit = "x"; }
                else if (settings_editor_index_ == 6) { unit = kUnitRevPerS2Ui; }
                break;
            case SettingsCategory::UI:
                if (settings_editor_index_ == 1) { unit = "%"; }
                break;
            case SettingsCategory::Main:
                break;
        }

        const bool has_unit = (unit[0] != '\0');
        switch (settings_editor_type_) {
            case SettingsEditorValueType::U32:
                if (settings_editor_category_ == SettingsCategory::FatigueTest && settings_editor_index_ == 4) {
                    const double old_s = static_cast<double>(settings_editor_u32_old_) * 0.1;
                    const double new_s = static_cast<double>(settings_editor_u32_new_) * 0.1;
                    snprintf(old_line, sizeof(old_line), "Old: %.1f %s", old_s, unit);
                    snprintf(new_line, sizeof(new_line), "New: %.1f %s", new_s, unit);
                } else if (has_unit) {
                    snprintf(old_line, sizeof(old_line), "Old: %" PRIu32 " %s", settings_editor_u32_old_, unit);
                    snprintf(new_line, sizeof(new_line), "New: %" PRIu32 " %s", settings_editor_u32_new_, unit);
                } else {
                    snprintf(old_line, sizeof(old_line), "Old: %" PRIu32, settings_editor_u32_old_);
                    snprintf(new_line, sizeof(new_line), "New: %" PRIu32, settings_editor_u32_new_);
                }
                break;
            case SettingsEditorValueType::F32:
                if (has_unit) {
                    snprintf(old_line, sizeof(old_line), "Old: %.1f %s", static_cast<double>(settings_editor_f32_old_), unit);
                    snprintf(new_line, sizeof(new_line), "New: %.1f %s", static_cast<double>(settings_editor_f32_new_), unit);
                } else {
                    snprintf(old_line, sizeof(old_line), "Old: %.1f", static_cast<double>(settings_editor_f32_old_));
                    snprintf(new_line, sizeof(new_line), "New: %.1f", static_cast<double>(settings_editor_f32_new_));
                }
                break;
            case SettingsEditorValueType::Bool:
                if (bool_is_mode) {
                    snprintf(old_line, sizeof(old_line), "Old: %s", settings_editor_bool_old_ ? "StallGuard" : "Encoder");
                    snprintf(new_line, sizeof(new_line), "New: %s", settings_editor_bool_new_ ? "StallGuard" : "Encoder");
                } else {
                    snprintf(old_line, sizeof(old_line), "Old: %s", settings_editor_bool_old_ ? "Yes" : "No");
                    snprintf(new_line, sizeof(new_line), "New: %s", settings_editor_bool_new_ ? "Yes" : "No");
                }
                break;
            case SettingsEditorValueType::U8: {
                const int old_pct = static_cast<int>(settings_editor_u8_old_) * 100 / 255;
                const int new_pct = static_cast<int>(settings_editor_u8_new_) * 100 / 255;
                snprintf(old_line, sizeof(old_line), "Old: %d%%", old_pct);
                snprintf(new_line, sizeof(new_line), "New: %d%%", new_pct);
                break;
            }
            default:
                snprintf(old_line, sizeof(old_line), "Old: -");
                snprintf(new_line, sizeof(new_line), "New: -");
                break;
        }

        canvas_->setTextSize(1);
        canvas_->setTextColor(colors::text_secondary);
        canvas_->setCursor(x + 16, y + 50);
        canvas_->print(old_line);
        canvas_->setCursor(x + 16, y + 68);
        canvas_->print(new_line);

        // Buttons
        const int16_t btn_w = 84;
        const int16_t btn_h = 32;
        const int16_t btn_y = y + h - 44;
        const Rect keep_btn{static_cast<int16_t>(cx - btn_w - 10), btn_y, btn_w, btn_h};
        const Rect disc_btn{static_cast<int16_t>(cx + 10), btn_y, btn_w, btn_h};
        drawButton_(keep_btn, "Keep", settings_popup_selection_ == 0, false);
        drawButton_(disc_btn, "Discard", settings_popup_selection_ == 1, false);
    } else if (settings_popup_mode_ == SettingsPopupMode::SaveConfirm) {
        canvas_->setTextSize(1);
        canvas_->setTextColor(colors::text_secondary);
        canvas_->setCursor(x + 16, y + 52);
        canvas_->print("Send edited settings to tester");
        canvas_->setCursor(x + 16, y + 70);
        canvas_->print("or re-sync from machine config");

        const int16_t btn_w = 84;
        const int16_t btn_h = 32;
        const int16_t btn_y = y + h - 44;
        const Rect send_btn{static_cast<int16_t>(cx - btn_w - 10), btn_y, btn_w, btn_h};
        const Rect sync_btn{static_cast<int16_t>(cx + 10), btn_y, btn_w, btn_h};
        drawButton_(send_btn, "Send", settings_popup_selection_ == 0, false);
        drawButton_(sync_btn, "Resync", settings_popup_selection_ == 1, false);
    }
}

void ui::UiController::drawBounds_(uint32_t now_ms) noexcept
{
    const int16_t cx = th::CENTER_X;
    const int16_t cy = th::CENTER_Y;

    static constexpr bool kSwingLeftFirst_ = true;

    // === BACKGROUND / CROSSHAIR ===
    canvas_->drawCircle(cx, cy, 96, colors::bg_card);
    canvas_->drawCircle(cx, cy, 66, colors::bg_card);
    canvas_->drawCircle(cx, cy, 38, colors::bg_card);
    canvas_->drawLine(cx - 100, cy, cx + 100, cy, colors::bg_card);
    canvas_->drawLine(cx, cy - 100, cx, cy + 100, colors::bg_card);

    // Title
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_primary);
    drawCenteredText_(cx, 10, "FIND BOUNDS", colors::text_primary, 1);

    // === STATUS TEXT ===
    const char* status1 = "READY";
    const char* status2 = "";
    uint16_t status_color = colors::text_secondary;
    if (bounds_state_ == BoundsState::StartWaitAck) {
        status1 = "STARTING";
        status2 = "Waiting for ACK";
        status_color = colors::text_hint;
    } else if (bounds_state_ == BoundsState::Running) {
        status1 = "RUNNING";
        status2 = "Finding mechanical limits";
        status_color = colors::accent_green;
    } else if (bounds_state_ == BoundsState::StopWaitAck) {
        status1 = "STOPPING";
        status2 = "Waiting for ACK";
        status_color = colors::text_hint;
    } else if (bounds_state_ == BoundsState::Complete) {
        status1 = bounds_have_result_ ? (bounds_bounded_ ? "BOUNDS FOUND" : "DEFAULT RANGE") : "DONE";
        status2 = bounds_have_result_ ? "Showing min/max" : "No data";
        status_color = colors::accent_blue;
    } else if (bounds_state_ == BoundsState::Error) {
        status1 = "CAN'T START";
        status2 = (bounds_last_error_code_ == 0) ? "No ACK from machine" : "Error from machine";
        status_color = colors::state_error;
    }

    drawCenteredText_(cx, 28, status1, status_color, 2);
    if (status2[0] != '\0') {
        drawCenteredText_(cx, 46, status2, colors::text_hint, 1);
    }

    // === VISUALIZATION (CROSSHAIR + TRACK) ===
    const int16_t track_y = cy + 16;
    const int16_t track_half_w = 72;
    const int16_t track_x1 = cx - track_half_w;
    const int16_t track_x2 = cx + track_half_w;
    canvas_->drawWideLine(track_x1, track_y, track_x2, track_y, 3, colors::bg_elevated);
    canvas_->fillSmoothCircle(cx, track_y, 4, colors::text_secondary);

    // Determine displayed bounds (if we have them)
    const bool show_bounds = (bounds_state_ == BoundsState::Complete) && bounds_have_result_;
    const float min_deg = bounds_min_deg_;
    const float max_deg = bounds_max_deg_;
    const float max_abs = std::max(1.0f, std::max(std::fabs(min_deg), std::fabs(max_deg)));
    const float display_max_deg = show_bounds ? max_abs : 75.0f;
    const float px_per_deg = static_cast<float>(track_half_w) / display_max_deg;

    int16_t min_x = cx;
    int16_t max_x = cx;
    if (show_bounds) {
        min_x = static_cast<int16_t>(cx + static_cast<int16_t>(min_deg * px_per_deg));
        max_x = static_cast<int16_t>(cx + static_cast<int16_t>(max_deg * px_per_deg));
        min_x = std::max(track_x1, std::min(track_x2, min_x));
        max_x = std::max(track_x1, std::min(track_x2, max_x));

        // Bounds markers
        canvas_->drawWideLine(min_x, track_y - 10, min_x, track_y + 10, 3, colors::accent_orange);
        canvas_->drawWideLine(max_x, track_y - 10, max_x, track_y + 10, 3, colors::accent_orange);

        // Highlight the usable window
        canvas_->drawWideLine(min_x, track_y, max_x, track_y, 5, colors::accent_blue);
    }

    // Armature indicator: rotational swing around center (starts left-first or right-first).
    float sim_angle_deg = 0.0f;
    if (bounds_state_ == BoundsState::Running || bounds_state_ == BoundsState::StartWaitAck || bounds_state_ == BoundsState::StopWaitAck) {
        const float t = static_cast<float>((now_ms - bounds_state_since_ms_) % 2400U) / 2400.0f;
        const float phase = 2.0f * 3.14159f * t;
        const float s = kSwingLeftFirst_ ? -std::cos(phase) : std::cos(phase);
        sim_angle_deg = s * 60.0f;
    } else if (show_bounds) {
        const float t = static_cast<float>((now_ms - bounds_state_since_ms_) % 3000U) / 3000.0f;
        const float phase = 2.0f * 3.14159f * t;
        const float s = kSwingLeftFirst_ ? -std::cos(phase) : std::cos(phase);
        sim_angle_deg = (min_deg + max_deg) * 0.5f + s * (max_deg - min_deg) * 0.5f;
    }

    const int16_t pivot_x = cx;
    const int16_t pivot_y = static_cast<int16_t>(cy - 6);
    const int16_t arm_len = 60;
    const float rad = sim_angle_deg * 3.14159f / 180.0f;
    const int16_t tip_x = static_cast<int16_t>(pivot_x + static_cast<int16_t>(arm_len * std::sin(rad)));
    const int16_t tip_y = static_cast<int16_t>(pivot_y - static_cast<int16_t>(arm_len * std::cos(rad)));
    canvas_->drawWideLine(pivot_x, pivot_y, tip_x, tip_y, 4, colors::text_primary);
    canvas_->fillSmoothCircle(pivot_x, pivot_y, 4, colors::bg_elevated);
    canvas_->drawCircle(pivot_x, pivot_y, 5, colors::bg_card);
    canvas_->fillSmoothCircle(tip_x, tip_y, 6, colors::accent_green);

    // Show where the armature maps onto the track (small dot)
    int16_t dot_x = static_cast<int16_t>(cx + static_cast<int16_t>(sim_angle_deg * px_per_deg));
    dot_x = std::max(track_x1, std::min(track_x2, dot_x));
    canvas_->fillSmoothCircle(dot_x, track_y, 3, colors::text_primary);

    // Numeric readout (only when we have results)
    if (show_bounds) {
        char buf1[32];
        char buf2[32];
        snprintf(buf1, sizeof(buf1), "MIN %.2f deg", static_cast<double>(min_deg));
        snprintf(buf2, sizeof(buf2), "MAX %.2f deg", static_cast<double>(max_deg));

        // Put the numbers on a dark pill so they stay readable even when the
        // blue window highlight passes behind them.
        canvas_->setTextSize(1);
        const int16_t y = 150;
        constexpr int16_t kPadX = 8;
        constexpr int16_t kPillH = 18;
        constexpr int16_t kRadius = 9;
        const auto draw_value_pill = [&](int16_t center_x, const char* text) {
            const int16_t tw = static_cast<int16_t>(canvas_->textWidth(text));
            const int16_t pill_w = static_cast<int16_t>(tw + (kPadX * 2));
            const int16_t x = static_cast<int16_t>(center_x - (pill_w / 2));
            canvas_->fillRoundRect(x, y, pill_w, kPillH, kRadius, colors::bg_card);
            canvas_->drawRoundRect(x, y, pill_w, kPillH, kRadius, colors::accent_orange);
            canvas_->setTextColor(colors::accent_orange);
            canvas_->setCursor(static_cast<int16_t>(x + kPadX), static_cast<int16_t>(y + 5));
            canvas_->print(text);
        };

        draw_value_pill(static_cast<int16_t>(cx - 56), buf1);
        draw_value_pill(static_cast<int16_t>(cx + 56), buf2);
    }

    // === BOTTOM CONTROLS (Back + Start/Stop) ===
    // Slightly higher to avoid bottom-edge clipping on the round display.
    const Rect back_btn{ 18, 186, 64, 30 };
    const Rect action_btn{ 90, 186, 132, 30 };

    const char* action_label = "Start";
    if (bounds_state_ == BoundsState::Running) action_label = "Stop";
    if (bounds_state_ == BoundsState::StartWaitAck) action_label = "Starting";
    if (bounds_state_ == BoundsState::StopWaitAck) action_label = "Stopping";
    if (bounds_state_ == BoundsState::Complete) action_label = "Run Again";
    if (bounds_state_ == BoundsState::Error) action_label = "Retry";

    drawModernButton_(back_btn.x, back_btn.y, back_btn.w, back_btn.h, "Back", bounds_focus_ == BoundsFocus::Back, false, colors::accent_blue);
    drawModernButton_(action_btn.x, action_btn.y, action_btn.w, action_btn.h, action_label, bounds_focus_ == BoundsFocus::Action, false, colors::accent_blue);

    // Connection indicator (top-right)
    th::drawConnectionDot(240 - 18, 18, conn_status_ == ConnStatus::Connected, now_ms);
}

void ui::UiController::drawLiveCounter_(uint32_t now_ms) noexcept
{
    // Check if popup is active
    if (live_popup_mode_ == LivePopupMode::QuickSettings) {
        drawQuickSettings_(now_ms);
        return;
    }
    if (live_popup_mode_ != LivePopupMode::None) {
        drawLivePopup_(now_ms);
        return;
    }

    const int16_t cx = th::CENTER_X;
    const int16_t cy = th::CENTER_Y;
    // Only use status if connected - don't show stale data when disconnected
    const bool use_status = (conn_status_ == ConnStatus::Connected && have_status_);
    const uint32_t cycle = use_status ? last_status_.cycle_number : 0;
    const uint32_t target = edit_settings_.test_unit.cycle_amount;
    const auto test_state = use_status ? static_cast<fatigue_proto::TestState>(last_status_.state) : fatigue_proto::TestState::Idle;

    // Check pending command timeout
    if (pending_command_id_ != 0 && (now_ms - pending_command_tick_ > 2500)) {
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
    // Back button (top-center; circular-safe)
    const int16_t back_x = 76;
    const int16_t back_y = 10;
    const int16_t back_w = 88;
    const int16_t back_h = 30;
    const bool back_focused = (live_focus_ == LiveFocus::Back);
    canvas_->fillSmoothRoundRect(back_x, back_y, back_w, back_h, 10, back_focused ? colors::accent_blue : colors::bg_elevated);
    if (back_focused) {
        canvas_->drawRoundRect(back_x, back_y, back_w, back_h, 10, colors::text_primary);
    }
    canvas_->setTextSize(1);
    canvas_->setTextColor(back_focused ? colors::bg_primary : colors::text_secondary);
    canvas_->setCursor(back_x + 18, back_y + 9);
    canvas_->print("< Back");

    // Connection indicator (top-right)
    th::drawConnectionDot(240 - 18, 18, conn_status_ == ConnStatus::Connected, now_ms);

    // === BOTTOM ACTION HINT ===
    const int16_t hint_y = 240 - 28;
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_hint);
    
    // Show different hints based on state
    const char* hint_text = (live_focus_ == LiveFocus::Back) ? "Press: back" : "Press: actions";
    if (test_state == fatigue_proto::TestState::Running || test_state == fatigue_proto::TestState::Paused) {
        // During test: show that long-press opens quick settings (abbreviated to fit)
        hint_text = (live_focus_ == LiveFocus::Back) ? "Press: back" : "Click: menu | Hold: cfg";
    }
    drawCenteredText_(cx, hint_y, hint_text, colors::text_hint, 1);
    
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
        
        drawButton_(cancel_btn, "Cancel", live_popup_selection_ == 0, false);
        drawActionButton_(btn_x2, btn_y1, btn_w, btn_h, "Start", 
                         live_popup_selection_ == 1, colors::accent_green, false);
        
    } else if (live_popup_mode_ == LivePopupMode::RunningActions) {
        // Three buttons: Back / Pause / Stop
        const int16_t btn_spacing = 8;
        const int16_t total_w = btn_w * 2 + btn_spacing;
        const int16_t btn_x1 = cx - total_w / 2;
        const int16_t btn_x2 = btn_x1 + btn_w + btn_spacing;
        
        const Rect back_btn{btn_x1, btn_y1, btn_w, btn_h};
        const int16_t stop_x = cx - btn_w / 2;
        
        drawButton_(back_btn, "Back", live_popup_selection_ == 0, false);
        drawActionButton_(btn_x2, btn_y1, btn_w, btn_h, "Pause",
                         live_popup_selection_ == 1, colors::accent_yellow, true);
        drawActionButton_(stop_x, btn_y2, btn_w, btn_h, "Stop",
                         live_popup_selection_ == 2, colors::accent_red, false);
        
    } else if (live_popup_mode_ == LivePopupMode::PausedActions) {
        // Three buttons: Back / Resume / Stop
        const int16_t btn_spacing = 8;
        const int16_t total_w = btn_w * 2 + btn_spacing;
        const int16_t btn_x1 = cx - total_w / 2;
        const int16_t btn_x2 = btn_x1 + btn_w + btn_spacing;
        
        const Rect back_btn{btn_x1, btn_y1, btn_w, btn_h};
        const int16_t stop_x = cx - btn_w / 2;
        
        drawButton_(back_btn, "Back", live_popup_selection_ == 0, false);
        drawActionButton_(btn_x2, btn_y1, btn_w, btn_h, "Resume",
                         live_popup_selection_ == 1, colors::accent_green, false);
        drawActionButton_(stop_x, btn_y2, btn_w, btn_h, "Stop",
                         live_popup_selection_ == 2, colors::accent_red, false);
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
                const bool ok = espnow::SendCommand(
                    fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
                    static_cast<uint8_t>(fatigue_proto::CommandId::Start),
                    nullptr,
                    0);
                if (ok) {
                    pending_command_id_ = 1;
                    pending_command_tick_ = now_ms;
                    logf_(now_ms, "TX: Start cmd");
                } else {
                    logf_(now_ms, "TX: Start cmd FAILED");
                }
                live_popup_mode_ = LivePopupMode::None;
            }
        } else if (live_popup_mode_ == LivePopupMode::RunningActions) {
            if (live_popup_selection_ == 0) {
                // Back
                live_popup_mode_ = LivePopupMode::None;
            } else if (live_popup_selection_ == 1) {
                // Pause
                const bool ok = espnow::SendCommand(
                    fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
                    static_cast<uint8_t>(fatigue_proto::CommandId::Pause),
                    nullptr,
                    0);
                if (ok) {
                    pending_command_id_ = 2;
                    pending_command_tick_ = now_ms;
                    logf_(now_ms, "TX: Pause cmd");
                } else {
                    logf_(now_ms, "TX: Pause cmd FAILED");
                }
                live_popup_mode_ = LivePopupMode::None;
            } else {
                // Stop
                const bool ok = espnow::SendCommand(
                    fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
                    static_cast<uint8_t>(fatigue_proto::CommandId::Stop),
                    nullptr,
                    0);
                if (ok) {
                    pending_command_id_ = 4;
                    pending_command_tick_ = now_ms;
                    logf_(now_ms, "TX: Stop cmd");
                } else {
                    logf_(now_ms, "TX: Stop cmd FAILED");
                }
                live_popup_mode_ = LivePopupMode::None;
            }
        } else if (live_popup_mode_ == LivePopupMode::PausedActions) {
            if (live_popup_selection_ == 0) {
                // Back
                live_popup_mode_ = LivePopupMode::None;
            } else if (live_popup_selection_ == 1) {
                // Resume
                const bool ok = espnow::SendCommand(
                    fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
                    static_cast<uint8_t>(fatigue_proto::CommandId::Resume),
                    nullptr,
                    0);
                if (ok) {
                    pending_command_id_ = 3;
                    pending_command_tick_ = now_ms;
                    logf_(now_ms, "TX: Resume cmd");
                } else {
                    logf_(now_ms, "TX: Resume cmd FAILED");
                }
                live_popup_mode_ = LivePopupMode::None;
            } else {
                // Stop
                const bool ok = espnow::SendCommand(
                    fatigue_proto::DEVICE_ID_FATIGUE_TESTER_,
                    static_cast<uint8_t>(fatigue_proto::CommandId::Stop),
                    nullptr,
                    0);
                if (ok) {
                    pending_command_id_ = 4;
                    pending_command_tick_ = now_ms;
                    logf_(now_ms, "TX: Stop cmd");
                } else {
                    logf_(now_ms, "TX: Stop cmd FAILED");
                }
                live_popup_mode_ = LivePopupMode::None;
            }
        }
        
        dirty_ = true;
    }
}

// ============================================================================
// Quick Settings - accessible via long-press on LiveCounter during test
// ============================================================================

void ui::UiController::drawQuickSettings_(uint32_t now_ms) noexcept
{
    (void)now_ms;
    
    const int16_t cx = th::CENTER_X;
    const int16_t cy = th::CENTER_Y;
    
    // Full-screen overlay with dark background
    canvas_->fillScreen(colors::bg_primary);
    canvas_->drawCircle(cx, cy, 118, colors::bg_elevated);
    
    // Title - use size 1 with bold simulation for compact fit
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_primary);
    const char* title = "Quick Config";
    int16_t tw = static_cast<int16_t>(canvas_->textWidth(title));
    // Draw twice offset by 1px for bold effect
    canvas_->setCursor(cx - tw / 2, 22);
    canvas_->print(title);
    canvas_->setCursor(cx - tw / 2 + 1, 22);
    canvas_->print(title);
    
    // Subtitle hint
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_hint);
    const char* hint = "Adjust mid-test";
    const int16_t hw = static_cast<int16_t>(canvas_->textWidth(hint));
    canvas_->setCursor(cx - hw / 2, 36);
    canvas_->print(hint);
    
    // Item layout: compact vertical list
    // Items: 0=< Back, 1=VMAX, 2=AMAX, 3=Dwell, 4=Cycles
    // Layout tuned for 240px circular display - items must end by ~200px to leave room for hint
    static constexpr int16_t kItemH = 30;
    static constexpr int16_t kListTop = 48;
    static constexpr int16_t kListW = 180;
    static constexpr int16_t kListX = (240 - kListW) / 2;
    
    const char* labels[kQuickSettingsItemCount_] = {
        "< Back",
        "VMAX",
        "AMAX", 
        "Dwell",
        "Cycles"
    };
    
    // Format current values
    char values[kQuickSettingsItemCount_][24];
    values[0][0] = '\0';  // Back has no value
    
    if (quick_settings_editing_ && quick_settings_index_ == 1) {
        snprintf(values[1], sizeof(values[1]), "%.1f RPM", static_cast<double>(quick_editor_f32_new_));
    } else {
        snprintf(values[1], sizeof(values[1]), "%.1f RPM", static_cast<double>(edit_settings_.test_unit.oscillation_vmax_rpm));
    }
    
    if (quick_settings_editing_ && quick_settings_index_ == 2) {
        snprintf(values[2], sizeof(values[2]), "%.2f", static_cast<double>(quick_editor_f32_new_));
    } else {
        snprintf(values[2], sizeof(values[2]), "%.2f", static_cast<double>(edit_settings_.test_unit.oscillation_amax_rev_s2));
    }
    
    if (quick_settings_editing_ && quick_settings_index_ == 3) {
        snprintf(values[3], sizeof(values[3]), "%.1f s", static_cast<double>(quick_editor_f32_new_));
    } else {
        const float dwell_sec = static_cast<float>(edit_settings_.test_unit.dwell_time_ms) / 1000.0f;
        snprintf(values[3], sizeof(values[3]), "%.1f s", static_cast<double>(dwell_sec));
    }
    
    if (quick_settings_editing_ && quick_settings_index_ == 4) {
        if (quick_editor_u32_new_ == 0) {
            snprintf(values[4], sizeof(values[4]), "Infinite");
        } else {
            snprintf(values[4], sizeof(values[4]), "%" PRIu32, quick_editor_u32_new_);
        }
    } else {
        if (edit_settings_.test_unit.cycle_amount == 0) {
            snprintf(values[4], sizeof(values[4]), "Infinite");
        } else {
            snprintf(values[4], sizeof(values[4]), "%" PRIu32, edit_settings_.test_unit.cycle_amount);
        }
    }
    
    for (int i = 0; i < kQuickSettingsItemCount_; ++i) {
        const int16_t y = kListTop + i * kItemH;
        const bool selected = (quick_settings_index_ == i);
        const bool editing = (quick_settings_editing_ && quick_settings_index_ == i);
        
        // Background - use smaller rounding for compact items
        if (selected) {
            canvas_->fillSmoothRoundRect(kListX, y, kListW, kItemH - 2, 6, 
                editing ? colors::accent_orange : colors::accent_blue);
        } else {
            canvas_->fillSmoothRoundRect(kListX, y, kListW, kItemH - 2, 6, colors::bg_elevated);
        }
        
        // Label - text size 2 for better readability
        canvas_->setTextSize(2);
        canvas_->setTextColor(selected ? colors::bg_primary : colors::text_secondary);
        canvas_->setCursor(kListX + 8, y + 5);
        canvas_->print(labels[i]);
        
        // Value (right-aligned)
        if (i > 0) {
            const int16_t vw = static_cast<int16_t>(canvas_->textWidth(values[i]));
            canvas_->setCursor(kListX + kListW - vw - 8, y + 5);
            canvas_->print(values[i]);
        }
    }
    
    // Bottom hint - show step info when editing, in a styled pill
    const char* action_hint;
    char hint_buf[48];
    if (quick_settings_editing_) {
        if (quick_editor_type_ == QuickEditorType::F32) {
            snprintf(hint_buf, sizeof(hint_buf), "Step:%.1f | Hold:step", static_cast<double>(quick_editor_f32_step_));
            action_hint = hint_buf;
        } else if (quick_editor_type_ == QuickEditorType::U32) {
            snprintf(hint_buf, sizeof(hint_buf), "Step:%lu | Hold:step", static_cast<unsigned long>(quick_editor_u32_step_));
            action_hint = hint_buf;
        } else {
            action_hint = "Rotate: adjust";
        }
    } else {
        action_hint = "Click:edit | Back:exit";
    }
    canvas_->setTextSize(1);
    const int16_t ahw = static_cast<int16_t>(canvas_->textWidth(action_hint));
    const int16_t pill_w = ahw + 16;  // padding on each side
    const int16_t pill_h = 18;
    const int16_t pill_x = cx - pill_w / 2;
    const int16_t pill_y = 205;
    // Draw pill background with border
    canvas_->fillSmoothRoundRect(pill_x, pill_y, pill_w, pill_h, 9, colors::bg_elevated);
    canvas_->drawRoundRect(pill_x, pill_y, pill_w, pill_h, 9, colors::text_hint);
    // Draw text centered in pill
    canvas_->setTextColor(colors::text_secondary);
    canvas_->setCursor(cx - ahw / 2, pill_y + 4);
    canvas_->print(action_hint);
    
    // Draw confirmation popup if active
    if (quick_settings_confirm_popup_) {
        // Popup overlay
        const int16_t pw = 180;
        const int16_t ph = 90;
        const int16_t px = cx - pw / 2;
        const int16_t py = cy - ph / 2;
        
        canvas_->fillSmoothRoundRect(px, py, pw, ph, 12, colors::bg_elevated);
        canvas_->drawRoundRect(px, py, pw, ph, 12, colors::accent_blue);
        
        // Title
        canvas_->setTextSize(2);
        canvas_->setTextColor(colors::text_primary);
        const char* popup_title = "Apply?";
        const int16_t ptw = static_cast<int16_t>(canvas_->textWidth(popup_title));
        canvas_->setCursor(cx - ptw / 2, py + 12);
        canvas_->print(popup_title);
        
        // Buttons: Keep / Revert
        const int16_t btn_w = 70;
        const int16_t btn_h = 32;
        const int16_t btn_y = py + ph - btn_h - 12;
        const int16_t btn_spacing = 10;
        const int16_t total_btn_w = btn_w * 2 + btn_spacing;
        const int16_t keep_x = cx - total_btn_w / 2;
        const int16_t revert_x = keep_x + btn_w + btn_spacing;
        
        // Keep button (green accent)
        drawActionButton_(keep_x, btn_y, btn_w, btn_h, "Keep",
                         quick_settings_confirm_sel_ == 0, colors::accent_green, false);
        
        // Revert button (red accent)
        drawActionButton_(revert_x, btn_y, btn_w, btn_h, "Revert",
                         quick_settings_confirm_sel_ == 1, colors::accent_red, false);
    }
}

void ui::UiController::handleQuickSettingsInput_(int delta, bool click, uint32_t now_ms) noexcept
{
    // Handle confirmation popup first
    if (quick_settings_confirm_popup_) {
        if (delta != 0) {
            quick_settings_confirm_sel_ = (quick_settings_confirm_sel_ == 0) ? 1 : 0;
            playBeep_(delta > 0 ? 1 : 0);
            dirty_ = true;
        }
        if (click) {
            playBeep_(2);
            if (quick_settings_confirm_sel_ == 0) {
                // Keep - apply and send
                applyQuickSettingsValue_(now_ms);
            } else {
                // Revert
                discardQuickSettingsValue_();
            }
            quick_settings_confirm_popup_ = false;
            quick_settings_confirm_sel_ = 0;
            quick_settings_editing_ = false;
            quick_editor_type_ = QuickEditorType::None;
            dirty_ = true;
        }
        return;
    }
    
    // Handle editing mode
    if (quick_settings_editing_) {
        if (delta != 0) {
            handleQuickSettingsValueEdit_(delta);
            dirty_ = true;
        }
        if (click) {
            // Check if value changed
            if (quickEditorHasChange_()) {
                // Show confirmation popup
                quick_settings_confirm_popup_ = true;
                quick_settings_confirm_sel_ = 0;  // Default to Keep
                playBeep_(2);
            } else {
                // No change, just exit editing
                quick_settings_editing_ = false;
                quick_editor_type_ = QuickEditorType::None;
                playBeep_(2);
            }
            dirty_ = true;
        }
        return;
    }
    
    // Normal navigation
    if (delta != 0) {
        quick_settings_index_ = (quick_settings_index_ + delta + kQuickSettingsItemCount_) % kQuickSettingsItemCount_;
        playBeep_(delta > 0 ? 1 : 0);
        dirty_ = true;
    }
    
    if (click) {
        if (quick_settings_index_ == 0) {
            // Back - exit quick settings
            live_popup_mode_ = LivePopupMode::None;
            playBeep_(2);
        } else {
            // Enter edit mode for this item
            beginQuickSettingsEdit_();
            playBeep_(2);
        }
        dirty_ = true;
    }
}

void ui::UiController::beginQuickSettingsEdit_() noexcept
{
    quick_settings_editing_ = true;
    quick_editor_type_ = QuickEditorType::None;
    
    switch (quick_settings_index_) {
        case 1:  // VMAX (F32)
            quick_editor_type_ = QuickEditorType::F32;
            quick_editor_f32_old_ = edit_settings_.test_unit.oscillation_vmax_rpm;
            quick_editor_f32_new_ = quick_editor_f32_old_;
            quick_editor_f32_step_ = 1.0f;  // 1 RPM steps
            break;
        case 2:  // AMAX (F32)
            quick_editor_type_ = QuickEditorType::F32;
            quick_editor_f32_old_ = edit_settings_.test_unit.oscillation_amax_rev_s2;
            quick_editor_f32_new_ = quick_editor_f32_old_;
            quick_editor_f32_step_ = 0.1f;  // 0.1 rev/s² steps
            break;
        case 3:  // Dwell (F32 in seconds)
            quick_editor_type_ = QuickEditorType::F32;
            quick_editor_f32_old_ = static_cast<float>(edit_settings_.test_unit.dwell_time_ms) / 1000.0f;
            quick_editor_f32_new_ = quick_editor_f32_old_;
            quick_editor_f32_step_ = 0.1f;  // 0.1 second steps
            break;
        case 4:  // Cycles (U32)
            quick_editor_type_ = QuickEditorType::U32;
            quick_editor_u32_old_ = edit_settings_.test_unit.cycle_amount;
            quick_editor_u32_new_ = quick_editor_u32_old_;
            quick_editor_u32_step_ = 10;  // Start with step of 10
            break;
        default:
            quick_settings_editing_ = false;
            break;
    }
}

void ui::UiController::handleQuickSettingsValueEdit_(int delta) noexcept
{
    if (!quick_settings_editing_ || delta == 0) {
        return;
    }
    
    playBeep_(delta > 0 ? 1 : 0);
    
    switch (quick_editor_type_) {
        case QuickEditorType::F32: {
            const float next = quick_editor_f32_new_ + quick_editor_f32_step_ * static_cast<float>(delta);
            quick_editor_f32_new_ = std::max(0.1f, std::round(next * 10.0f) / 10.0f);
            break;
        }
        case QuickEditorType::U32: {
            const int64_t next = static_cast<int64_t>(quick_editor_u32_new_) + static_cast<int64_t>(delta) * static_cast<int64_t>(quick_editor_u32_step_);
            quick_editor_u32_new_ = static_cast<uint32_t>(std::max(int64_t{0}, next));
            break;
        }
        default:
            break;
    }
}

bool ui::UiController::quickEditorHasChange_() const noexcept
{
    switch (quick_editor_type_) {
        case QuickEditorType::F32:
            return std::fabs(static_cast<double>(quick_editor_f32_new_ - quick_editor_f32_old_)) > 0.001;
        case QuickEditorType::U32:
            return quick_editor_u32_new_ != quick_editor_u32_old_;
        default:
            return false;
    }
}

void ui::UiController::cycleQuickSettingsStep_() noexcept
{
    // Handle F32 types (VMAX, AMAX)
    if (quick_editor_type_ == QuickEditorType::F32) {
        // VMAX and AMAX both use 0.1, 1, 10 steps
        static constexpr float kSteps[] = {0.1f, 1.0f, 10.0f};
        static constexpr size_t kCount = sizeof(kSteps) / sizeof(kSteps[0]);
        
        size_t idx = 0;
        for (size_t i = 0; i < kCount; ++i) {
            if (std::fabs(static_cast<double>(kSteps[i] - quick_editor_f32_step_)) < 1e-6) {
                idx = i;
                break;
            }
        }
        idx = (idx + 1) % kCount;
        quick_editor_f32_step_ = kSteps[idx];
    }
    // Handle U32 types (Cycles only - Dwell is now F32)
    else if (quick_editor_type_ == QuickEditorType::U32) {
        if (quick_settings_index_ == 4) {
            // Cycles: 10, 100, 1000 steps
            static constexpr uint32_t kCycleSteps[] = {10, 100, 1000};
            static constexpr size_t kCycleCount = sizeof(kCycleSteps) / sizeof(kCycleSteps[0]);
            
            size_t idx = 0;
            for (size_t i = 0; i < kCycleCount; ++i) {
                if (kCycleSteps[i] == quick_editor_u32_step_) {
                    idx = i;
                    break;
                }
            }
            idx = (idx + 1) % kCycleCount;
            quick_editor_u32_step_ = kCycleSteps[idx];
        }
    }
}

void ui::UiController::applyQuickSettingsValue_(uint32_t now_ms) noexcept
{
    // Apply the value to edit_settings_
    switch (quick_settings_index_) {
        case 1:  // VMAX
            edit_settings_.test_unit.oscillation_vmax_rpm = std::max(5.0f, quick_editor_f32_new_);
            break;
        case 2:  // AMAX
            edit_settings_.test_unit.oscillation_amax_rev_s2 = std::max(0.5f, quick_editor_f32_new_);
            break;
        case 3:  // Dwell (convert seconds to ms)
            edit_settings_.test_unit.dwell_time_ms = static_cast<uint32_t>(std::max(0.0f, quick_editor_f32_new_) * 1000.0f);
            break;
        case 4:  // Cycles
            edit_settings_.test_unit.cycle_amount = quick_editor_u32_new_;
            break;
        default:
            return;
    }
    
    // Also update the main settings
    if (settings_ != nullptr) {
        *settings_ = edit_settings_;
        (void)SettingsStore::Save(*settings_);
    }
    
    // Send config to unit immediately
    if (conn_status_ == ConnStatus::Connected) {
        const auto payload = fatigue_proto::BuildConfigPayload(edit_settings_);
        const bool ok = espnow::SendConfigSet(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_, &payload, sizeof(payload));
        if (ok) {
            logf_(now_ms, "TX: Quick config update sent");
        } else {
            logf_(now_ms, "TX: Quick config FAILED");
        }
    } else {
        logf_(now_ms, "TX: Quick config skipped (not connected)");
    }
}

void ui::UiController::discardQuickSettingsValue_() noexcept
{
    // Just reset the editor values - no changes applied
    switch (quick_editor_type_) {
        case QuickEditorType::F32:
            quick_editor_f32_new_ = quick_editor_f32_old_;
            break;
        case QuickEditorType::U32:
            quick_editor_u32_new_ = quick_editor_u32_old_;
            break;
        default:
            break;
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
    
    // Scroll indicator (left edge, circular-bound style + springy ends)
    if (log_count_ > static_cast<size_t>(max_lines)) {
        const int max_scroll = std::max(0, static_cast<int>(log_count_) - max_lines);
        const float scroll_pos = (max_scroll > 0)
            ? (1.0f - static_cast<float>(scroll_lines_) / static_cast<float>(max_scroll))
            : 1.0f;

        const int16_t arc_top = static_cast<int16_t>(log_top + 8);
        const int16_t arc_bottom = static_cast<int16_t>(log_bottom - 8);

        int16_t dot_y = static_cast<int16_t>(arc_top + scroll_pos * static_cast<float>(arc_bottom - arc_top));
        dot_y = std::clamp(dot_y, arc_top, arc_bottom);

        const float r = 110.0f;
        const float cy_arc = 120.0f;
        const float dy = static_cast<float>(dot_y) - cy_arc;
        const float dx = std::sqrt(std::max(0.0f, r * r - dy * dy));
        const int16_t dot_x = 120 - static_cast<int16_t>(dx);

        // Dot
        canvas_->fillSmoothCircle(dot_x, dot_y, 4, colors::accent_blue);

        // End markers
        const float dy_top = static_cast<float>(arc_top) - cy_arc;
        const float dx_top = std::sqrt(std::max(0.0f, r * r - dy_top * dy_top));
        const int16_t x_top = 120 - static_cast<int16_t>(dx_top);
        const float dy_bot = static_cast<float>(arc_bottom) - cy_arc;
        const float dx_bot = std::sqrt(std::max(0.0f, r * r - dy_bot * dy_bot));
        const int16_t x_bot = 120 - static_cast<int16_t>(dx_bot);
        canvas_->fillCircle(x_top, arc_top, 1, colors::text_hint);
        canvas_->fillCircle(x_bot, arc_bottom, 1, colors::text_hint);

        // Springy end feedback
        if (terminal_overscroll_px_ != 0.0f) {
            const bool at_bottom = terminal_overscroll_px_ > 0.0f;
            const int16_t spring_y = at_bottom ? arc_bottom : arc_top;
            const float dy_s = static_cast<float>(spring_y) - cy_arc;
            const float dx_s = std::sqrt(std::max(0.0f, r * r - dy_s * dy_s));
            const int16_t spring_x = 120 - static_cast<int16_t>(dx_s);

            const float amp = std::min(10.0f, std::fabs(terminal_overscroll_px_));
            const int16_t rr = static_cast<int16_t>(4 + (amp * 0.25f));
            canvas_->drawCircle(spring_x, spring_y, rr + 2, colors::accent_blue);
        }

        // Decay overscroll spring
        terminal_overscroll_px_ *= 0.72f;
        if (std::fabs(terminal_overscroll_px_) < 0.25f) {
            terminal_overscroll_px_ = 0.0f;
        }
    } else {
        terminal_overscroll_px_ = 0.0f;
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
