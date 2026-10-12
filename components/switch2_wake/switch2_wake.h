#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/button/button.h"

#ifdef USE_ESP32
#include "esphome/components/esp32_ble/ble.h"
#include <esp_gap_ble_api.h>
#endif

namespace esphome::switch2_wake {

// Sends the BLE "wake" advertisement that a paired controller emits when its
// HOME button is pressed while the Nintendo Switch 2 is asleep.
class Switch2Wake : public Component, public Parented<esp32_ble::ESP32BLE> {
 public:
  void setup() override;
  void dump_config() override;
  // Must run before the BLE stack starts: the Bluetooth address is fixed at init.
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_controller_address(uint64_t address);
  void set_console_address(uint64_t address);
  void set_product_id(uint16_t product_id) { this->product_id_ = product_id; }
  void set_duration(uint32_t ms) { this->duration_ms_ = ms; }
  void set_interval(uint32_t ms) { this->interval_ms_ = ms; }

  // Starts one advertising burst. Ignored if a burst is already running.
  void send();
  bool is_sending() const { return this->state_ != State::IDLE; }

#ifdef USE_ESP32
  // Called by esp32_ble for every GAP event (registered from __init__.py).
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#endif

 protected:
  enum class State : uint8_t { IDLE, CONFIGURING, ADVERTISING };

  void build_payload_();

  uint8_t controller_address_[6]{};
  uint8_t console_address_[6]{};
  uint16_t product_id_{0x2066};
  uint32_t duration_ms_{2000};
  uint32_t interval_ms_{20};
  uint8_t payload_[31]{};
  State state_{State::IDLE};
};

class WakeButton : public button::Button, public Parented<Switch2Wake> {
 protected:
  void press_action() override { this->parent_->send(); }
};

template<typename... Ts> class SendAction final : public Action<Ts...>, public Parented<Switch2Wake> {
 public:
  void play(const Ts &...x) override { this->parent_->send(); }
};

}  // namespace esphome::switch2_wake
