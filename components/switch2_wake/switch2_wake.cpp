#include "switch2_wake.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_mac.h>
#endif

namespace esphome::switch2_wake {

static const char *const TAG = "switch2_wake";

// Nintendo's Bluetooth SIG company identifier and USB vendor ID.
static constexpr uint16_t NINTENDO_COMPANY_ID = 0x0553;
static constexpr uint16_t NINTENDO_VENDOR_ID = 0x057E;

// Bluetooth addresses are written most-significant byte first in the config
// (AA:BB:CC:DD:EE:FF); keep them in that order here.
static void unpack_address(uint64_t address, uint8_t out[6]) {
  for (int i = 0; i < 6; i++)
    out[i] = (address >> (8 * (5 - i))) & 0xFF;
}

void Switch2Wake::set_controller_address(uint64_t address) { unpack_address(address, this->controller_address_); }
void Switch2Wake::set_console_address(uint64_t address) { unpack_address(address, this->console_address_); }

// 31-byte legacy advertisement:
//   [0..2]   Flags AD structure (LE General Discoverable, BR/EDR not supported)
//   [3..4]   Manufacturer Specific Data AD header (length 27)
//   [5..6]   Company ID 0x0553, little endian
//   [7..9]   01 00 03
//   [10..11] USB vendor ID 0x057E, little endian
//   [12..13] Controller product ID, little endian
//   [14..16] 00 01 81 -- 0x81 = "reconnect from sleep"
//   [17..22] Console Bluetooth address, least-significant byte first
//   [23]     0F
//   [24..30] zero padding
void Switch2Wake::build_payload_() {
  uint8_t *p = this->payload_;
  memset(p, 0, sizeof(this->payload_));
  p[0] = 0x02;
  p[1] = 0x01;
  p[2] = 0x06;
  p[3] = 0x1B;
  p[4] = 0xFF;
  p[5] = NINTENDO_COMPANY_ID & 0xFF;
  p[6] = NINTENDO_COMPANY_ID >> 8;
  p[7] = 0x01;
  p[8] = 0x00;
  p[9] = 0x03;
  p[10] = NINTENDO_VENDOR_ID & 0xFF;
  p[11] = NINTENDO_VENDOR_ID >> 8;
  p[12] = this->product_id_ & 0xFF;
  p[13] = this->product_id_ >> 8;
  p[14] = 0x00;
  p[15] = 0x01;
  p[16] = 0x81;
  for (int i = 0; i < 6; i++)
    p[17 + i] = this->console_address_[5 - i];
  p[23] = 0x0F;
}

void Switch2Wake::setup() {
  this->build_payload_();
#ifdef USE_ESP32
  // The console only accepts the beacon from a bonded controller address, so
  // the ESP32's Bluetooth address becomes the controller's. Only the Bluetooth
  // interface changes; Wi-Fi keeps the factory MAC (and its DHCP lease).
  esp_err_t err = esp_iface_mac_addr_set(this->controller_address_, ESP_MAC_BT);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Could not set the Bluetooth address (%s)", esp_err_to_name(err));
    this->mark_failed();
  }
#endif
}

void Switch2Wake::dump_config() {
  ESP_LOGCONFIG(TAG, "Switch 2 Wake:");
  const uint8_t *c = this->controller_address_;
  const uint8_t *s = this->console_address_;
  ESP_LOGCONFIG(TAG, "  Controller address: %02X:%02X:%02X:%02X:%02X:%02X", c[0], c[1], c[2], c[3], c[4], c[5]);
  ESP_LOGCONFIG(TAG, "  Console address: %02X:%02X:%02X:%02X:%02X:%02X", s[0], s[1], s[2], s[3], s[4], s[5]);
  ESP_LOGCONFIG(TAG, "  Product ID: 0x%04X", this->product_id_);
  ESP_LOGCONFIG(TAG, "  Burst: %" PRIu32 " ms every %" PRIu32 " ms", this->duration_ms_, this->interval_ms_);
#ifdef USE_ESP32
  uint8_t bt[6];
  if (esp_read_mac(bt, ESP_MAC_BT) == ESP_OK) {
    ESP_LOGCONFIG(TAG, "  Active Bluetooth address: %02X:%02X:%02X:%02X:%02X:%02X", bt[0], bt[1], bt[2], bt[3],
                  bt[4], bt[5]);
  }
#endif
}

void Switch2Wake::send() {
#ifdef USE_ESP32
  if (this->is_failed())
    return;
  if (this->parent_ == nullptr || !this->parent_->is_active()) {
    ESP_LOGW(TAG, "BLE is not active yet, wake skipped");
    return;
  }
  if (this->state_ != State::IDLE) {
    ESP_LOGD(TAG, "A burst is already running, wake skipped");
    return;
  }
  ESP_LOGI(TAG, "Sending wake burst");
  // Raw data is applied asynchronously; advertising starts once the stack
  // confirms it (ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT).
  esp_err_t err = esp_ble_gap_config_adv_data_raw(this->payload_, sizeof(this->payload_));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Setting advertisement data failed (%s)", esp_err_to_name(err));
    return;
  }
  this->state_ = State::CONFIGURING;
#endif
}

#ifdef USE_ESP32
void Switch2Wake::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      if (this->state_ != State::CONFIGURING)
        return;
      if (param->adv_data_raw_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Advertisement data rejected (status %d)", param->adv_data_raw_cmpl.status);
        this->state_ = State::IDLE;
        return;
      }
      // Advertising interval is expressed in 0.625 ms units.
      uint16_t units = (this->interval_ms_ * 1000) / 625;
      esp_ble_adv_params_t params{};
      params.adv_int_min = units;
      params.adv_int_max = units;
      params.adv_type = ADV_TYPE_IND;
      params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
      params.channel_map = ADV_CHNL_ALL;
      params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
      esp_err_t err = esp_ble_gap_start_advertising(&params);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Starting advertising failed (%s)", esp_err_to_name(err));
        this->state_ = State::IDLE;
      }
      break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      if (this->state_ != State::CONFIGURING)
        return;
      if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Advertising did not start (status %d)", param->adv_start_cmpl.status);
        this->state_ = State::IDLE;
        return;
      }
      this->state_ = State::ADVERTISING;
      this->set_timeout("stop", this->duration_ms_, []() { esp_ble_gap_stop_advertising(); });
      break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      if (this->state_ == State::ADVERTISING) {
        ESP_LOGD(TAG, "Wake burst finished");
        this->state_ = State::IDLE;
      }
      break;
    default:
      break;
  }
}
#endif

}  // namespace esphome::switch2_wake
