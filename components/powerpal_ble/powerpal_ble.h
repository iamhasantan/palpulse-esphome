#pragma once

#include "accumulator.h"
#include "wire.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_API
#include "esphome/components/api/custom_api_device.h"
#endif

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#ifdef USE_ESP32
#include <esp_gattc_api.h>
#include <nvs.h>
#include <array>
#include <cstdint>
#include <cstddef>

namespace esphome::powerpal_ble {

namespace espbt = esphome::esp32_ble_tracker;

class PalPulse : public Component, public ble_client::BLEClientNode
#ifdef USE_API
    , public api::CustomAPIDevice
#endif
{
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t interface,
                           esp_ble_gattc_cb_param_t *params) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *params) override;

  void set_pairing_code(uint32_t value) { pairing_code_ = value; }
  void set_pulses_per_kwh(float value) { pulses_per_kwh_ = value; }
  void set_notification_interval(uint8_t minutes) { interval_minutes_ = minutes; }
  void set_live_power(bool enabled) { wants_live_power_ = enabled; }
  void set_history_events(bool enabled) { history_events_enabled_ = enabled; }
  void set_power_sensor(sensor::Sensor *sensor) { power_ = sensor; }
  void set_energy_sensor(sensor::Sensor *sensor) { energy_ = sensor; }
  void set_daily_energy_sensor(sensor::Sensor *sensor) { daily_ = sensor; }
  void set_battery_sensor(sensor::Sensor *sensor) { battery_ = sensor; }
#ifdef USE_TIME
  void set_time(time::RealTimeClock *source) { clock_ = source; }
#endif

 private:
  enum class Phase : uint8_t { OFFLINE, PAIRING, CLOCK, INTERVAL, SUBSCRIBING, READY };
  enum class Request : uint8_t { NONE, BOUNDS, STREAM };
  struct Attributes {
    uint16_t pairing{0};
    uint16_t interval{0};
    uint16_t clock{0};
    uint16_t bounds{0};
    uint16_t request{0};
    uint16_t records{0};
    uint16_t live{0};
    uint16_t battery{0};
  };

  void clear_connection_();
  bool discover_();
  bool write_characteristic_(uint16_t handle, uint8_t *buffer, uint16_t length);
  void begin_pairing_();
  bool write_clock_if_valid_();
  void configure_interval_();
  void subscribe_records_();
  void finish_subscription_();
  void ready_();
  void poll_();
  void schedule_poll_(uint32_t milliseconds);
  void read_bounds_();
  void handle_bounds_(const uint8_t *bytes, size_t length);
  void handle_record_(const uint8_t *bytes, size_t length);
  void handle_live_(const uint8_t *bytes, size_t length);
  void stream_complete_();
  void request_timeout_();
  void load_checkpoint_();
  bool save_checkpoint_();
  void publish_energy_(bool caught_up);
  uint32_t local_day_(uint32_t timestamp) const;
  // Native-API event transport is best-effort. Only emit records after their
  // matching cumulative counter has been durably committed to NVS.
  struct QueuedRecord {
    uint64_t total_pulses{0};
    uint32_t timestamp{0};
    uint16_t pulses{0};
  };
  static constexpr size_t HISTORY_QUEUE_SIZE = 512;
  void queue_history_record_(uint32_t timestamp, uint16_t pulses, uint64_t total);

  Phase phase_{Phase::OFFLINE};
  Request request_{Request::NONE};
  Attributes attr_{};
  uint32_t pairing_code_{0};
  float pulses_per_kwh_{0.0f};
  uint8_t interval_minutes_{5};
  bool wants_live_power_{false};
  bool history_events_enabled_{false};
  bool history_sender_warned_{false};
  std::array<QueuedRecord, HISTORY_QUEUE_SIZE> history_queue_{};
  size_t history_head_{0};
  size_t history_tail_{0};
  size_t history_count_{0};
  uint32_t last_history_emit_ms_{0};
  bool live_subscribed_{false};
  bool clock_set_{false};
  bool pairing_pending_{false};
  bool initialized_{false};
  bool checkpoint_dirty_{false};
  bool nvs_ready_{false};
  uint32_t first_record_{0};
  uint32_t newest_record_{0};
  uint32_t active_start_{0};
  uint32_t active_end_{0};
  uint32_t records_seen_{0};
  nvs_handle_t nvs_{0};
  accounting::Checkpoint counter_{};

  sensor::Sensor *power_{nullptr};
  sensor::Sensor *energy_{nullptr};
  sensor::Sensor *daily_{nullptr};
  sensor::Sensor *battery_{nullptr};
#ifdef USE_TIME
  time::RealTimeClock *clock_{nullptr};
#endif
};

}  // namespace esphome::powerpal_ble
#endif
