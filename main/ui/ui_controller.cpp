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
    
    // Premium boot splash + fade-in animation (drawn on sprite)
    constexpr int kFadeSteps = 40;
    for (int i = 0; i <= kFadeSteps; ++i) {
        const uint32_t t_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        const float p = static_cast<float>(i) / static_cast<float>(kFadeSteps);

        drawBootScreen_(t_ms, p);
        if (canvas_ != nullptr) {
            canvas_->pushSprite(0, 0);
        }

        // Keep the original "fade to half brightness" behavior (0..128)
        const uint8_t b = static_cast<uint8_t>(std::min(128.0f, 128.0f * p));
        M5.Display.setBrightness(b);
        vTaskDelay(pdMS_TO_TICKS(12));
    }
    vTaskDelay(pdMS_TO_TICKS(650));  // Hold boot splash briefly

    // Option A: fade out to black before handing off to the real UI.
    // (Avoids a harsh brightness jump and prevents a boot-splash "flash".)
    constexpr int kFadeOutSteps = 24;
    for (int i = kFadeOutSteps; i >= 0; --i) {
        const float p = static_cast<float>(i) / static_cast<float>(kFadeOutSteps);
        const uint8_t b = static_cast<uint8_t>(std::min(128.0f, 128.0f * p));
        M5.Display.setBrightness(b);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // Ensure we don't briefly reveal the splash again when restoring brightness.
    if (canvas_ != nullptr) {
        canvas_->fillScreen(TFT_BLACK);
        canvas_->pushSprite(0, 0);
    } else {
        M5.Display.fillScreen(TFT_BLACK);
    }

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

void ui::UiController::drawBootScreen_(uint32_t now_ms, float progress) noexcept
{
    if (canvas_ == nullptr) {
        return;
    }

    progress = std::max(0.0f, std::min(1.0f, progress));
    const float ease = easeOutCubic_(progress);

    // --- Background: subtle vertical gradient (deep navy -> black) ---
    for (int16_t y = 0; y < SCREEN_SIZE_; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(SCREEN_SIZE_ - 1);
        const uint8_t r = static_cast<uint8_t>(4 + 10 * (1.0f - t));
        const uint8_t g = static_cast<uint8_t>(6 + 14 * (1.0f - t));
        const uint8_t b = static_cast<uint8_t>(12 + 26 * (1.0f - t));
        const uint16_t c = colors::rgb565(r, g, b);
        canvas_->fillRect(0, y, SCREEN_SIZE_, 1, c);
    }

    // Circular vignette ring to emphasize the dial display
    canvas_->drawCircle(CENTER_X_, CENTER_Y_, 118, colors::bg_secondary);
    canvas_->drawCircle(CENTER_X_, CENTER_Y_, 119, colors::text_hint);

    // --- Glow behind the logo ---
    const int16_t glow_x = CENTER_X_;
    const int16_t glow_y = CENTER_Y_ - 18;
    const float pulse = 0.5f + 0.5f * std::sinf(static_cast<float>(now_ms) * 0.006f);
    const uint8_t glow_boost = static_cast<uint8_t>(18 + 28 * pulse);
    for (int i = 0; i < 6; ++i) {
        const int16_t r = static_cast<int16_t>(92 - i * 14);
        const uint8_t rr = static_cast<uint8_t>(0 + i * 2);
        const uint8_t gg = static_cast<uint8_t>(22 + glow_boost + i * 6);
        const uint8_t bb = static_cast<uint8_t>(48 + glow_boost + i * 10);
        canvas_->fillSmoothCircle(glow_x, glow_y, r, colors::rgb565(rr, gg, bb));
    }

    // --- Progress ring (boot "loading" feel) ---
    const float ring_p = 0.08f + 0.92f * ease;
    drawProgressArc_(CENTER_X_, CENTER_Y_, 112, 7, ring_p, colors::accent_cyan, colors::progress_bg);

    // Accent orbit dot along the ring
    const float ang = (-90.0f + 360.0f * ring_p) * (3.1415926f / 180.0f);
    const int16_t dot_x = static_cast<int16_t>(CENTER_X_ + std::cos(ang) * 112.0f);
    const int16_t dot_y = static_cast<int16_t>(CENTER_Y_ + std::sin(ang) * 112.0f);
    canvas_->fillSmoothCircle(dot_x, dot_y, 4, colors::accent_blue);
    canvas_->drawCircle(dot_x, dot_y, 5, colors::text_hint);

    // --- Logo block (shadowed) ---
    const int16_t logo_y = CENTER_Y_ - 36;
    canvas_->setTextSize(3);
    canvas_->setTextColor(colors::bg_secondary);
    canvas_->drawCenterString("ConMed", CENTER_X_ + 2, logo_y + 2);
    canvas_->setTextColor(TFT_WHITE);
    canvas_->drawCenterString("ConMed", CENTER_X_, logo_y);

    // TM badge (small pill/bubble)
    const int16_t tm_x = CENTER_X_ + 70;
    const int16_t tm_y = logo_y - 10;
    canvas_->fillSmoothRoundRect(tm_x - 12, tm_y - 7, 26, 16, 8, colors::bg_card);
    canvas_->drawRoundRect(tm_x - 12, tm_y - 7, 26, 16, 8, colors::text_hint);
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_secondary);
    canvas_->setCursor(tm_x - 6, tm_y - 3);
    canvas_->print("TM");

    // --- Subtitle pill ---
    const char* subtitle = "Fatigue Test Unit";
    canvas_->setTextSize(1);
    const int16_t sub_w = static_cast<int16_t>(canvas_->textWidth(subtitle)) + 26;
    const int16_t sub_h = 20;
    const int16_t sub_x = CENTER_X_ - sub_w / 2;
    const int16_t sub_y = CENTER_Y_ + 18;
    canvas_->fillSmoothRoundRect(sub_x, sub_y, sub_w, sub_h, sub_h / 2, colors::bg_card);
    canvas_->drawRoundRect(sub_x, sub_y, sub_w, sub_h, sub_h / 2, colors::button_border);
    canvas_->setTextColor(colors::text_secondary);
    canvas_->setCursor(sub_x + 13, sub_y + 6);
    canvas_->print(subtitle);

    // --- Status line ---
    canvas_->setTextColor(colors::text_muted);
    canvas_->setCursor(18, 216);
    canvas_->print("Starting...");

    // Small progress hint (percent)
    char pct[8];
    const int percent = static_cast<int>(std::lround(progress * 100.0f));
    std::snprintf(pct, sizeof(pct), "%d%%", std::max(0, std::min(100, percent)));
    canvas_->setCursor(240 - 18 - static_cast<int16_t>(canvas_->textWidth(pct)), 216);
    canvas_->print(pct);
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
                    logf_(now_ms, "RX: ConfigResponse cycles=%" PRIu32 " t=%" PRIu32 "s dwell=%" PRIu32 "s", cfg.cycle_amount,
                          cfg.time_per_cycle_sec, cfg.dwell_time_sec);
                    dirty_ = true;
                }
                break;
            }
            case espnow::MsgType::CommandAck: {
                logf_(now_ms, "RX: CommandAck");

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
        logf_(now_ms, "Connection timeout - reconnecting");
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
        int item_count = 4;
        switch (settings_category_) {
            case SettingsCategory::Main: item_count = 4; break;
            case SettingsCategory::FatigueTest: item_count = 4; break;
            case SettingsCategory::BoundsFinding: item_count = 6; break;
            case SettingsCategory::UI: item_count = 3; break;
        }

        settings_index_ = std::clamp(settings_index_ + delta, 0, item_count - 1);
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
                settings_index_ = 0;
            }
            dirty_ = true;
            return;
        }

        // Main: enter sub-category.
        if (settings_category_ == SettingsCategory::Main) {
            switch (settings_index_) {
                case 1: settings_category_ = SettingsCategory::FatigueTest; break;
                case 2: settings_category_ = SettingsCategory::BoundsFinding; break;
                case 3: settings_category_ = SettingsCategory::UI; break;
                default: break;
            }
            settings_index_ = 1;
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
    if (page_ != Page::Landing && page_ != Page::Bounds) {
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
        const Rect back_btn{ 18, 190, 64, 32 };
        const Rect action_btn{ 90, 190, 132, 32 };
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
    settings_category_ = SettingsCategory::Main;
    settings_focus_ = SettingsFocus::List;
    settings_value_editing_ = false;

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

void ui::UiController::settingsBack_() noexcept
{
    // Discard changes, return to landing.
    in_settings_edit_ = false;
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

    // Push config to test unit.
    const auto payload = fatigue_proto::BuildConfigPayload(*settings_);
    (void)espnow::SendConfigSet(fatigue_proto::DEVICE_ID_FATIGUE_TESTER_, &payload, sizeof(payload));
    logf_(now_ms, "TX: ConfigSet dev=%u", fatigue_proto::DEVICE_ID_FATIGUE_TESTER_);

    in_settings_edit_ = false;
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
                } else if (settings_editor_index_ == 2) {
                    edit_settings_.test_unit.time_per_cycle_sec = settings_editor_u32_new_;
                } else if (settings_editor_index_ == 3) {
                    edit_settings_.test_unit.dwell_time_sec = settings_editor_u32_new_;
                }
            }
            break;

        case SettingsCategory::BoundsFinding:
            if (settings_editor_index_ == 1 && settings_editor_type_ == SettingsEditorValueType::Bool) {
                edit_settings_.test_unit.bounds_method_stallguard = settings_editor_bool_new_;
            } else if (settings_editor_type_ == SettingsEditorValueType::F32) {
                if (settings_editor_index_ == 2) {
                    edit_settings_.test_unit.bounds_search_velocity_rpm = std::max(0.0f, settings_editor_f32_new_);
                } else if (settings_editor_index_ == 3) {
                    edit_settings_.test_unit.stallguard_min_velocity_rpm = std::max(0.0f, settings_editor_f32_new_);
                } else if (settings_editor_index_ == 4) {
                    edit_settings_.test_unit.stall_detection_current_factor = std::max(0.0f, settings_editor_f32_new_);
                } else if (settings_editor_index_ == 5) {
                    edit_settings_.test_unit.bounds_search_accel_rev_s2 = std::max(0.0f, settings_editor_f32_new_);
                }
            }
            break;

        case SettingsCategory::UI:
            if (settings_editor_index_ == 1 && settings_editor_type_ == SettingsEditorValueType::U8) {
                edit_settings_.ui.brightness = settings_editor_u8_new_;
                // Preview immediately
                M5.Display.setBrightness(edit_settings_.ui.brightness);
            } else if (settings_editor_index_ == 2 && settings_editor_type_ == SettingsEditorValueType::Bool) {
                edit_settings_.ui.orientation_flipped = settings_editor_bool_new_;
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

    // Snapshot the current value for the selected item.
    switch (settings_category_) {
        case SettingsCategory::FatigueTest:
            settings_editor_type_ = SettingsEditorValueType::U32;
            if (settings_index_ == 1) {
                settings_editor_u32_old_ = edit_settings_.test_unit.cycle_amount;
            } else if (settings_index_ == 2) {
                settings_editor_u32_old_ = edit_settings_.test_unit.time_per_cycle_sec;
            } else if (settings_index_ == 3) {
                settings_editor_u32_old_ = edit_settings_.test_unit.dwell_time_sec;
            } else {
                settings_editor_u32_old_ = 0;
            }
            settings_editor_u32_new_ = settings_editor_u32_old_;
            break;

        case SettingsCategory::BoundsFinding:
            if (settings_index_ == 1) {
                settings_editor_type_ = SettingsEditorValueType::Bool;
                settings_editor_bool_old_ = edit_settings_.test_unit.bounds_method_stallguard;
                settings_editor_bool_new_ = settings_editor_bool_old_;
            } else {
                settings_editor_type_ = SettingsEditorValueType::F32;
                if (settings_index_ == 2) {
                    settings_editor_f32_old_ = edit_settings_.test_unit.bounds_search_velocity_rpm;
                } else if (settings_index_ == 3) {
                    settings_editor_f32_old_ = edit_settings_.test_unit.stallguard_min_velocity_rpm;
                } else if (settings_index_ == 4) {
                    settings_editor_f32_old_ = edit_settings_.test_unit.stall_detection_current_factor;
                } else if (settings_index_ == 5) {
                    settings_editor_f32_old_ = edit_settings_.test_unit.bounds_search_accel_rev_s2;
                } else {
                    settings_editor_f32_old_ = 0.0f;
                }
                settings_editor_f32_new_ = settings_editor_f32_old_;
            }
            break;

        case SettingsCategory::UI:
            if (settings_index_ == 1) {
                settings_editor_type_ = SettingsEditorValueType::U8;
                settings_editor_u8_old_ = edit_settings_.ui.brightness;
                settings_editor_u8_new_ = settings_editor_u8_old_;
            } else if (settings_index_ == 2) {
                settings_editor_type_ = SettingsEditorValueType::Bool;
                settings_editor_bool_old_ = edit_settings_.ui.orientation_flipped;
                settings_editor_bool_new_ = settings_editor_bool_old_;
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
            const uint32_t step = (settings_editor_category_ == SettingsCategory::FatigueTest && settings_editor_index_ == 1) ? 10 : 1;
            settings_editor_u32_new_ = clamp_add_u32(settings_editor_u32_new_, delta, step);
            break;
        }
        case SettingsEditorValueType::F32: {
            float step = 0.1f;
            if (settings_editor_category_ == SettingsCategory::BoundsFinding && settings_editor_index_ == 4) {
                step = 0.05f;
            }
            settings_editor_f32_new_ = std::max(0.0f, settings_editor_f32_new_ + step * static_cast<float>(delta));
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
        default:
            break;
    }
}

void ui::UiController::handleSettingsPopupInput_(int delta, bool click, uint32_t now_ms) noexcept
{
    if (settings_popup_mode_ == SettingsPopupMode::None) {
        return;
    }

    const int max_sel = (settings_popup_mode_ == SettingsPopupMode::ValueChangeConfirm) ? 1 : 2;

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

    // SaveConfirm (legacy): 0=SAVE, 1=DISCARD, 2=CANCEL
    if (settings_popup_selection_ == 0) {
        settingsSave_(now_ms);
        return;
    }
    if (settings_popup_selection_ == 1) {
        settingsBack_();
        return;
    }

    // Cancel
    settings_popup_mode_ = SettingsPopupMode::None;
    settings_popup_selection_ = 0;
    dirty_ = true;
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
            snprintf(values[2], sizeof(values[2]), "%" PRIu32 " s", edit_settings_.test_unit.time_per_cycle_sec);
            snprintf(values[3], sizeof(values[3]), "%" PRIu32 " s", edit_settings_.test_unit.dwell_time_sec);
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
            snprintf(values[5], sizeof(values[5]), "%.1f rev/s^2", static_cast<double>(edit_settings_.test_unit.bounds_search_accel_rev_s2));
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
        const bool editing = false; // No inline editing; value changes happen in a dedicated editor screen.
        
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

    switch (settings_editor_category_) {
        case SettingsCategory::FatigueTest:
            if (settings_editor_index_ == 1) { label = "Cycles"; }
            else if (settings_editor_index_ == 2) { label = "Time/Cycle"; unit = "s"; }
            else if (settings_editor_index_ == 3) { label = "Dwell Time"; unit = "s"; }
            break;
        case SettingsCategory::BoundsFinding:
            if (settings_editor_index_ == 1) { label = "Mode"; bool_is_mode = true; }
            else if (settings_editor_index_ == 2) { label = "Search Speed"; unit = "rpm"; }
            else if (settings_editor_index_ == 3) { label = "SG Min Vel"; unit = "rpm"; }
            else if (settings_editor_index_ == 4) { label = "Stall Factor"; unit = "x"; }
            else if (settings_editor_index_ == 5) { label = "Search Accel"; unit = "rev/s^2"; }
            break;
        case SettingsCategory::UI:
            if (settings_editor_index_ == 1) { label = "Brightness"; unit = "%"; }
            else if (settings_editor_index_ == 2) { label = "Flip Display"; }
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
    const bool has_unit = (unit[0] != '\0');
    switch (settings_editor_type_) {
        case SettingsEditorValueType::U32:
            if (has_unit) {
                snprintf(old_buf, sizeof(old_buf), "Old: %" PRIu32 " %s", settings_editor_u32_old_, unit);
                snprintf(new_buf, sizeof(new_buf), "%" PRIu32 " %s", settings_editor_u32_new_, unit);
            } else {
                snprintf(old_buf, sizeof(old_buf), "Old: %" PRIu32, settings_editor_u32_old_);
                snprintf(new_buf, sizeof(new_buf), "%" PRIu32, settings_editor_u32_new_);
            }
            break;
        case SettingsEditorValueType::F32:
            if (has_unit) {
                snprintf(old_buf, sizeof(old_buf), "Old: %.2f %s", static_cast<double>(settings_editor_f32_old_), unit);
                snprintf(new_buf, sizeof(new_buf), "%.2f %s", static_cast<double>(settings_editor_f32_new_), unit);
            } else {
                snprintf(old_buf, sizeof(old_buf), "Old: %.2f", static_cast<double>(settings_editor_f32_old_));
                snprintf(new_buf, sizeof(new_buf), "%.2f", static_cast<double>(settings_editor_f32_new_));
            }
            break;
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
    const int16_t vw = static_cast<int16_t>(canvas_->textWidth(new_buf));
    canvas_->setCursor(cx - vw / 2, cy - 22);
    canvas_->print(new_buf);

    // Instructions
    canvas_->setTextSize(1);
    canvas_->setTextColor(colors::text_hint);
    drawCenteredText_(cx, 196, "Rotate to change", colors::text_hint, 1);
    drawCenteredText_(cx, 212, "Press to finish", colors::text_hint, 1);
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

    const char* title = (settings_popup_mode_ == SettingsPopupMode::ValueChangeConfirm) ? "Keep change?" : "Settings";
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
                if (settings_editor_index_ == 2) { unit = "s"; }
                else if (settings_editor_index_ == 3) { unit = "s"; }
                break;
            case SettingsCategory::BoundsFinding:
                if (settings_editor_index_ == 1) { bool_is_mode = true; }
                else if (settings_editor_index_ == 2) { unit = "rpm"; }
                else if (settings_editor_index_ == 3) { unit = "rpm"; }
                else if (settings_editor_index_ == 4) { unit = "x"; }
                else if (settings_editor_index_ == 5) { unit = "rev/s^2"; }
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
                if (has_unit) {
                    snprintf(old_line, sizeof(old_line), "Old: %" PRIu32 " %s", settings_editor_u32_old_, unit);
                    snprintf(new_line, sizeof(new_line), "New: %" PRIu32 " %s", settings_editor_u32_new_, unit);
                } else {
                    snprintf(old_line, sizeof(old_line), "Old: %" PRIu32, settings_editor_u32_old_);
                    snprintf(new_line, sizeof(new_line), "New: %" PRIu32, settings_editor_u32_new_);
                }
                break;
            case SettingsEditorValueType::F32:
                if (has_unit) {
                    snprintf(old_line, sizeof(old_line), "Old: %.2f %s", static_cast<double>(settings_editor_f32_old_), unit);
                    snprintf(new_line, sizeof(new_line), "New: %.2f %s", static_cast<double>(settings_editor_f32_new_), unit);
                } else {
                    snprintf(old_line, sizeof(old_line), "Old: %.2f", static_cast<double>(settings_editor_f32_old_));
                    snprintf(new_line, sizeof(new_line), "New: %.2f", static_cast<double>(settings_editor_f32_new_));
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
        snprintf(buf1, sizeof(buf1), "MIN %.1f\xB0", static_cast<double>(min_deg));
        snprintf(buf2, sizeof(buf2), "MAX %.1f\xB0", static_cast<double>(max_deg));
        canvas_->setTextSize(1);
        drawCenteredText_(cx - 48, 132, buf1, colors::accent_orange, 1);
        drawCenteredText_(cx + 48, 132, buf2, colors::accent_orange, 1);
    }

    // === BOTTOM CONTROLS (Back + Start/Stop) ===
    const Rect back_btn{ 18, 190, 64, 32 };
    const Rect action_btn{ 90, 190, 132, 32 };

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
