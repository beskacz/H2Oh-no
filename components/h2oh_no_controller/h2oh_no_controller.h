#pragma once

#include <array>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/preferences.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace h2oh_no_controller {

static constexpr const char *TAG = "h2oh_no_controller";

class H2OhNoController : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_status_led_pin(GPIOPin *pin);
  void set_valve(uint8_t index, switch_::Switch *sw);
  void set_led(uint8_t index, switch_::Switch *sw);
  void set_max_runtime(uint8_t index, number::Number *n);

  void emergency_stop();
  void toggle_selected();
  void refresh_leds();
  /// Three visible pulses on the selected LED vs its resting (valve-active) state.
  void blink_selection_indicator();

  void button_up();
  void button_down();
  void toggle_service_mode();

  bool is_service_mode() const { return this->service_mode_; }
  uint8_t valve_mask() const { return this->valve_mask_; }
  int selected_valve() const { return this->selected_; }

  bool allow_multiple() const { return this->allow_multiple_; }
  void set_allow_multiple(bool allow);

 protected:
  void apply_mask_to_valves_();
  void sync_mask_from_valves_();
  void apply_mask_to_leds_();
  void install_valve_callbacks_();
  void on_valve_state_change_();
  void save_allow_multiple_pref_();

  void cancel_all_valve_watchdogs_();
  void sync_watchdog_timers_(uint8_t prev_mask, uint8_t new_mask);
  void watchdog_close_valve_(uint8_t index);
  uint32_t valve_max_runtime_ms_(uint8_t index);

  void arm_selection_idle_timeout_();

  void selection_blink_run_step_();

  static uint8_t lowest_set_bit_mask_(uint8_t m);

  std::array<switch_::Switch *, 8> valves_{};
  std::array<switch_::Switch *, 8> leds_{};
  std::array<number::Number *, 8> max_runtimes_{};

  GPIOPin *status_led_pin_{nullptr};

  uint8_t valve_mask_{0};
  int selected_{0};
  bool service_mode_{false};
  bool allow_multiple_{true};

  ESPPreferenceObject pref_allow_multiple_;
  bool valve_callbacks_installed_{false};
  bool updating_valves_{false};

  /// Selection highlight animation (restarted by blink_selection_indicator).
  uint8_t selection_blink_led_index_{0};
  uint8_t selection_blink_step_{0};
  bool selection_blink_resting_on_{false};
};

}  // namespace h2oh_no_controller
}  // namespace esphome
