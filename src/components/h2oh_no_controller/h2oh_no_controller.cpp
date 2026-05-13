#include "h2oh_no_controller.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace h2oh_no_controller {

/// Service-mode status LED: three short flashes then long pause (distinct from ESPHome ERROR/WARNING).
static constexpr uint32_t STATUS_LED_SVC_ON_MS = 130;
static constexpr uint32_t STATUS_LED_SVC_GAP_MS = 170;
static constexpr uint32_t STATUS_LED_SVC_CYCLE_MS =
    3 * (STATUS_LED_SVC_ON_MS + STATUS_LED_SVC_GAP_MS) + 1100;

static bool status_led_service_pattern_on_(uint32_t ms_mod_cycle) {
  constexpr uint32_t unit = STATUS_LED_SVC_ON_MS + STATUS_LED_SVC_GAP_MS;
  constexpr uint32_t triple_ms = 3 * unit;
  if (ms_mod_cycle >= triple_ms)
    return false;
  return (ms_mod_cycle % unit) < STATUS_LED_SVC_ON_MS;
}

/// No panel button activity resets manual selection to valve 1 (index 0).
static constexpr uint32_t SELECTION_IDLE_MS = 30000;
/// Pulses when moving selection: inverted phase vs resting state, then restore (×3).
static constexpr uint8_t SELECTION_BLINK_PULSES = 3;
static constexpr uint32_t SELECTION_BLINK_PHASE_MS = 130;

static const char *const WD_NAMES[8] = {"h2wd0", "h2wd1", "h2wd2", "h2wd3", "h2wd4", "h2wd5", "h2wd6", "h2wd7"};

void H2OhNoController::set_status_led_pin(GPIOPin *pin) { this->status_led_pin_ = pin; }

void H2OhNoController::set_valve(uint8_t index, switch_::Switch *sw) {
  if (index < 8)
    this->valves_[index] = sw;
}
void H2OhNoController::set_led(uint8_t index, switch_::Switch *sw) {
  if (index < 8)
    this->leds_[index] = sw;
}
void H2OhNoController::setup() {
  if (this->status_led_pin_ != nullptr) {
    this->status_led_pin_->setup();
    this->status_led_pin_->digital_write(false);
  }
  this->install_valve_callbacks_();
}

void H2OhNoController::dump_config() {
  ESP_LOGCONFIG(TAG, "H2Oh-no valve controller");
  ESP_LOGCONFIG(TAG, "  Valve watchdog: safety fuse per valve (covers stuck-on / panel use; sprinkler sets normal duration)");
  ESP_LOGCONFIG(TAG, "  Valve mask at boot: 0x%02X", this->valve_mask_);
  ESP_LOGCONFIG(TAG, "  Selection idle timeout: %u s (exit selection; next Up/Down picks valve 1 / 8)",
                (unsigned) (SELECTION_IDLE_MS / 1000));
  if (this->status_led_pin_ != nullptr) {
    LOG_PIN("  Status LED pin (service + ESPHome status): ", this->status_led_pin_);
  }
}

void H2OhNoController::loop() {
  if (this->status_led_pin_ == nullptr)
    return;

  const uint32_t ms = millis();

  if (this->service_mode_) {
    this->status_led_pin_->digital_write(status_led_service_pattern_on_(ms % STATUS_LED_SVC_CYCLE_MS));
    return;
  }

  const uint8_t st = App.get_app_state();
  if ((st & STATUS_LED_ERROR) != 0) {
    this->status_led_pin_->digital_write(ms % 250u < 150u);
  } else if ((st & STATUS_LED_WARNING) != 0) {
    this->status_led_pin_->digital_write(ms % 1500u < 250u);
  } else {
    this->status_led_pin_->digital_write(false);
  }
}

void H2OhNoController::install_valve_callbacks_() {
  if (this->valve_callbacks_installed_)
    return;
  this->valve_callbacks_installed_ = true;
  for (auto *v : this->valves_) {
    if (v == nullptr)
      continue;
    v->add_on_state_callback([this](bool) { this->on_valve_state_change_(); });
  }
}

void H2OhNoController::cancel_all_valve_watchdogs_() {
  for (const char *name : WD_NAMES)
    this->cancel_timeout(name);
}

void H2OhNoController::sync_watchdog_timers_(uint8_t prev_mask, uint8_t new_mask) {
  for (int i = 0; i < 8; i++) {
    uint8_t bit = (uint8_t)(1 << i);
    bool was = (prev_mask & bit) != 0;
    bool now = (new_mask & bit) != 0;
    if (was && !now)
      this->cancel_timeout(WD_NAMES[i]);
    if (!was && now) {
      uint32_t ms = this->valve_max_runtime_ms_((uint8_t) i);
      this->cancel_timeout(WD_NAMES[i]);
      const uint8_t idx = (uint8_t) i;
      this->set_timeout(WD_NAMES[i], ms, [this, idx]() { this->watchdog_close_valve_(idx); });
      ESP_LOGD(TAG, "Watchdog armed valve %u for %u ms", (unsigned) (i + 1), (unsigned) ms);
    }
  }
}

uint32_t H2OhNoController::valve_max_runtime_ms_(uint8_t index) {
  if (index >= 8)
    return 0;
  // Long fuse only: normal zone timing comes from ESPHome `sprinkler`. This prevents a stuck valve
  // if software glitches; keep above any realistic `run_duration_number` * multiplier.
  static constexpr uint32_t kMaxMs = 90000 * 1000u;  // 25 hours
  return kMaxMs;
}

void H2OhNoController::watchdog_close_valve_(uint8_t index) {
  if (index >= 8)
    return;
  uint8_t bit = (uint8_t)(1 << index);
  if ((this->valve_mask_ & bit) == 0)
    return;
  ESP_LOGW(TAG, "Watchdog closed valve %u (safety fuse)", (unsigned) (index + 1));
  uint8_t prev = this->valve_mask_;
  this->valve_mask_ &= (uint8_t)~bit;
  this->sync_watchdog_timers_(prev, this->valve_mask_);
  this->apply_mask_to_valves_();
  this->apply_mask_to_leds_();
}

void H2OhNoController::on_valve_state_change_() {
  if (this->updating_valves_)
    return;
  uint8_t prev = this->valve_mask_;
  this->sync_mask_from_valves_();

  if (this->service_mode_ && this->valve_mask_ != 0) {
    ESP_LOGD(TAG, "Service mode active — closing valves opened via HA/hardware");
    this->valve_mask_ = 0;
    this->cancel_all_valve_watchdogs_();
    this->apply_mask_to_valves_();
    this->apply_mask_to_leds_();
    return;
  }

  this->sync_watchdog_timers_(prev, this->valve_mask_);
  this->apply_mask_to_leds_();
}

void H2OhNoController::sync_mask_from_valves_() {
  uint8_t m = 0;
  for (int i = 0; i < 8; i++) {
    if (this->valves_[i] != nullptr && this->valves_[i]->state)
      m |= (uint8_t)(1 << i);
  }
  this->valve_mask_ = m;
}

void H2OhNoController::apply_mask_to_valves_() {
  this->updating_valves_ = true;
  for (int i = 0; i < 8; i++) {
    if (this->valves_[i] == nullptr)
      continue;
    bool on = (this->valve_mask_ & (uint8_t)(1 << i)) != 0;
    if (on)
      this->valves_[i]->turn_on();
    else
      this->valves_[i]->turn_off();
  }
  this->updating_valves_ = false;
}

void H2OhNoController::apply_mask_to_leds_() {
  for (int i = 0; i < 8; i++) {
    if (this->leds_[i] == nullptr)
      continue;
    if ((this->valve_mask_ & (uint8_t)(1 << i)) != 0)
      this->leds_[i]->turn_on();
    else
      this->leds_[i]->turn_off();
  }
}

void H2OhNoController::arm_selection_idle_timeout_() {
  this->cancel_timeout("sel_idle");
  this->set_timeout("sel_idle", SELECTION_IDLE_MS, [this]() {
    if (this->selected_ >= 0 && this->selected_ <= 7)
      ESP_LOGD(TAG, "Panel idle — exited manual selection (next Up starts at valve 1)");
    this->selected_ = -1;
  });
}

void H2OhNoController::emergency_stop() {
  this->cancel_timeout("blink_sel");
  this->cancel_timeout("sel_idle");
  this->cancel_all_valve_watchdogs_();
  this->valve_mask_ = 0;
  this->selected_ = 0;
  this->apply_mask_to_valves_();
  this->apply_mask_to_leds_();
}

void H2OhNoController::toggle_selected() {
  if (this->service_mode_)
    return;

  this->arm_selection_idle_timeout_();

  uint8_t prev = this->valve_mask_;
  int s = this->selected_;
  if (s < 0 || s > 7)
    s = 0;
  bool active = (this->valve_mask_ & (uint8_t)(1 << s)) != 0;

  if (active) {
    this->valve_mask_ &= (uint8_t) ~(1 << s);
  } else {
    this->valve_mask_ |= (uint8_t)(1 << s);
  }

  this->sync_watchdog_timers_(prev, this->valve_mask_);
  this->apply_mask_to_valves_();
  this->apply_mask_to_leds_();
}

void H2OhNoController::refresh_leds() {
  this->sync_mask_from_valves_();
  this->apply_mask_to_leds_();
}

void H2OhNoController::blink_selection_indicator() {
  this->cancel_timeout("blink_sel");
  int s = this->selected_;
  if (s < 0 || s > 7 || this->leds_[s] == nullptr)
    return;

  this->selection_blink_led_index_ = (uint8_t) s;
  this->selection_blink_resting_on_ = (this->valve_mask_ & (uint8_t)(1 << s)) != 0;
  this->selection_blink_step_ = 0;
  this->selection_blink_run_step_();
}

void H2OhNoController::selection_blink_run_step_() {
  uint8_t s = this->selection_blink_led_index_;
  if (s > 7 || this->leds_[s] == nullptr) {
    this->apply_mask_to_leds_();
    return;
  }

  const uint8_t total_phases = SELECTION_BLINK_PULSES * 2;
  if (this->selection_blink_step_ >= total_phases) {
    this->apply_mask_to_leds_();
    return;
  }

  bool invert = (this->selection_blink_step_ % 2) == 0;
  bool on = invert ? !this->selection_blink_resting_on_ : this->selection_blink_resting_on_;
  if (on)
    this->leds_[s]->turn_on();
  else
    this->leds_[s]->turn_off();

  this->selection_blink_step_++;
  this->set_timeout("blink_sel", SELECTION_BLINK_PHASE_MS, [this]() { this->selection_blink_run_step_(); });
}

void H2OhNoController::button_up() {
  if (this->service_mode_)
    return;
  this->arm_selection_idle_timeout_();
  if (this->selected_ < 0 || this->selected_ > 7) {
    this->selected_ = 0;
    return;
  }
  this->selected_++;
  if (this->selected_ > 7)
    this->selected_ = 0;
}

void H2OhNoController::button_down() {
  if (this->service_mode_)
    return;
  this->arm_selection_idle_timeout_();
  if (this->selected_ < 0 || this->selected_ > 7) {
    this->selected_ = 7;
    return;
  }
  this->selected_--;
  if (this->selected_ < 0)
    this->selected_ = 7;
}

void H2OhNoController::toggle_service_mode() {
  this->arm_selection_idle_timeout_();
  this->service_mode_ = !this->service_mode_;
}

}  // namespace h2oh_no_controller
}  // namespace esphome
