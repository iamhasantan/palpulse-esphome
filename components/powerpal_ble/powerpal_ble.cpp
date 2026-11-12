#include "powerpal_ble.h"

#ifdef USE_ESP32
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <esp_err.h>
#include <nvs_flash.h>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <inttypes.h>

namespace esphome::powerpal_ble {
namespace {
static const char *const TAG = "palpulse";

using espbt::ESPBTUUID;
constexpr char POWERPAL_BASE[] = "59DAABCD-12F4-25A6-7D4F-55961DCE4205";

ESPBTUUID characteristic(uint16_t slot) {
  // Protocol UUID family: 59DAxxxx-12F4-25A6-7D4F-55961DCE4205.
  char uuid[37];
  snprintf(uuid, sizeof(uuid), "59DA%04X-12F4-25A6-7D4F-55961DCE4205", slot);
  return ESPBTUUID::from_raw(uuid);
}
}  // namespace

void PalPulse::setup() {
  this->counter_.device_key = accounting::meter_identity_hash(this->parent()->address_str());
  this->counter_.pulses_per_kwh = this->pulses_per_kwh_;
  this->load_checkpoint_();
  if (this->reliable_history_enabled_) {
    this->load_history_ack_();
#ifdef USE_API_CUSTOM_SERVICES
    char service_name[32]{};
    snprintf(service_name, sizeof(service_name), "palpulse_ack_%08" PRIx32,
             this->counter_.device_key);
    this->register_service(&PalPulse::ack_history_, service_name, {"timestamp"});
    ESP_LOGI(TAG, "History ACK action suffix: %s", service_name);
    snprintf(service_name, sizeof(service_name), "palpulse_replay_%08" PRIx32,
             this->counter_.device_key);
    this->register_service(&PalPulse::replay_from_checkpoint_, service_name);
    ESP_LOGI(TAG, "Safe replay action suffix: %s", service_name);
#else
    ESP_LOGE(TAG, "Reliable history requires api.custom_services: true");
    this->history_replay_fault_ = true;
#endif
  }
}

void PalPulse::queue_history_record_(uint32_t timestamp, uint16_t pulses, uint64_t total) {
  if (!this->history_events_enabled_ || this->history_replay_fault_)
    return;
  if (this->reliable_history_enabled_) {
    if (timestamp <= this->history_ack_.timestamp)
      return;
    if (this->history_count_ != 0) {
      const auto &previous = this->history_queue_[
          (this->history_tail_ + HISTORY_QUEUE_SIZE - 1) % HISTORY_QUEUE_SIZE];
      if (timestamp <= previous.timestamp)
        return;
    }
  }
  if (this->history_count_ == HISTORY_QUEUE_SIZE) {
    ESP_LOGW(TAG, "Timestamp history event queue full; dropping record %" PRIu32 ". "
                  "Companion importer will refuse incomplete hours", timestamp);
    return;
  }
  auto &slot = this->history_queue_[this->history_tail_];
  slot.timestamp = timestamp;
  slot.pulses = pulses;
  slot.total_pulses = total;
  this->history_tail_ = (this->history_tail_ + 1) % HISTORY_QUEUE_SIZE;
  ++this->history_count_;
}

void PalPulse::loop() {
  // Periodic read-only checkpoint report. It is useful even when no native
  // measurements have arrived. Never report an unsaved cumulative checkpoint.
#ifdef USE_API_HOMEASSISTANT_SERVICES
  const uint32_t now = millis();
  if (this->history_events_enabled_ && this->is_connected() &&
      !this->checkpoint_dirty_ &&
      (!this->last_checkpoint_report_ms_ ||
       now - this->last_checkpoint_report_ms_ >= 60000u)) {
    this->last_checkpoint_report_ms_ = now ? now : 1u;
    this->publish_checkpoint_status_();
  }
#endif
  if (!this->history_events_enabled_ || this->history_count_ == 0)
    return;
#ifdef USE_API_HOMEASSISTANT_SERVICES
  if (!this->is_connected() || this->checkpoint_dirty_)
    return;
  const uint32_t emit_now = millis();
  if (emit_now - this->last_history_emit_ms_ < (this->reliable_history_enabled_ ? 1000u : 120u))
    return;
  const QueuedRecord &record = this->history_queue_[this->history_head_];
  this->fire_homeassistant_event("esphome.palpulse_record", {
      {"meter", this->parent()->address_str()},
      {"timestamp", std::to_string(record.timestamp)},
      {"pulses", std::to_string(record.pulses)},
      {"total_pulses", std::to_string(record.total_pulses)},
      {"pulses_per_kwh", std::to_string(this->pulses_per_kwh_)},
  });
  if (!this->reliable_history_enabled_) {
    this->history_head_ = (this->history_head_ + 1) % HISTORY_QUEUE_SIZE;
    --this->history_count_;
  }
  // Reliable mode retains the front event until Home Assistant confirms its
  // per-meter durable ledger write through the registered native action.
  this->last_history_emit_ms_ = emit_now;
#else
  if (!this->history_sender_warned_) {
    ESP_LOGW(TAG, "Timestamped history disabled: add api.homeassistant_services: true "
                  "and allow Home Assistant actions for the ESPHome node");
    this->history_sender_warned_ = true;
  }
#endif
}

void PalPulse::publish_checkpoint_status_() {
#ifdef USE_API_HOMEASSISTANT_SERVICES
  // Informational only. These values come from the saved local counter and
  // the separate durable ACK cursor; HA must never treat this as a command
  // to change a Recorder statistic or to overwrite the firmware NVS.
  if (!this->history_events_enabled_ || !this->is_connected())
    return;
  this->fire_homeassistant_event("esphome.palpulse_checkpoint", {
      {"meter", this->parent()->address_str()},
      {"checkpoint_timestamp", std::to_string(this->counter_.last_timestamp)},
      {"total_pulses", std::to_string(this->counter_.total_pulses)},
      {"ack_timestamp", std::to_string(
          this->reliable_history_enabled_ ? this->history_ack_.timestamp : 0u)},
      {"ack_total_pulses", std::to_string(
          this->reliable_history_enabled_ ? this->history_ack_.total_pulses : 0u)},
      {"queue_size", std::to_string(this->history_count_)},
      {"reliable_history", this->reliable_history_enabled_ ? "true" : "false"},
      {"history_events", this->history_events_enabled_ ? "true" : "false"},
      {"nvs_ready", this->nvs_ready_ ? "true" : "false"},
      {"checkpoint_dirty", this->checkpoint_dirty_ ? "true" : "false"},
      {"replay_fault", this->history_replay_fault_ ? "true" : "false"},
      {"buffer_oldest", std::to_string(this->first_record_)},
      {"buffer_newest", std::to_string(this->newest_record_)},
  });
#endif
}

void PalPulse::replay_from_checkpoint_() {
  // This is a re-read request, NOT a cursor reset. Keep a replay fault
  // blocked rather than discarding unread meter history or fabricating it.
  if (!this->reliable_history_enabled_ || !this->history_events_enabled_ ||
      !this->nvs_ready_ || this->history_replay_fault_ ||
      this->checkpoint_dirty_) {
    ESP_LOGW(TAG, "Safe checkpoint replay refused: reliable=%d nvs=%d fault=%d dirty=%d",
             this->reliable_history_enabled_, this->nvs_ready_,
             this->history_replay_fault_, this->checkpoint_dirty_);
    return;
  }
  if (this->history_count_ != 0) {
    ESP_LOGI(TAG, "History queue already awaiting acknowledgment; keeping cursor unchanged");
    return;
  }
  if (this->phase_ != Phase::READY) {
    ESP_LOGW(TAG, "Safe replay deferred until BLE connection is ready");
    return;
  }
  ESP_LOGI(TAG, "Safe replay requested from acknowledged timestamp %" PRIu32,
           this->history_ack_.timestamp);
  this->schedule_poll_(1000);
}

void PalPulse::dump_config() {
  ESP_LOGCONFIG(TAG, "PalPulse Powerpal BLE");
  ESP_LOGCONFIG(TAG, "  Local BLE meter: %s", this->parent()->address_str());
  ESP_LOGCONFIG(TAG, "  Reading interval: %u minutes", this->interval_minutes_);
  ESP_LOGCONFIG(TAG, "  Live pulse updates: %s", this->wants_live_power_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Timestamped HA events: %s", this->history_events_enabled_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Confirmed event replay: %s", this->reliable_history_enabled_ ? "enabled" : "disabled");
  LOG_SENSOR("  ", "Power", this->power_);
  LOG_SENSOR("  ", "Total energy", this->energy_);
  LOG_SENSOR("  ", "Daily energy", this->daily_);
  LOG_SENSOR("  ", "Battery", this->battery_);
}

void PalPulse::load_checkpoint_() {
  // Never erase NVS; preserve user data even when a legacy blob cannot be read.
  auto err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS unavailable (%d); energy publication will be withheld", err);
    return;
  }
  err = nvs_open("freshpowerpal", NVS_READWRITE, &this->nvs_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cannot open legacy-compatible NVS namespace (%d)", err);
    return;
  }
  this->nvs_ready_ = true;
  accounting::Checkpoint candidate{};
  size_t size = sizeof(candidate);
  err = nvs_get_blob(this->nvs_, "checkpoint", &candidate, &size);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    // New devices use per-meter NVS from the outset. The original namespace
    // remains reserved for legacy checkpoints so two new meters never collide.
    nvs_close(this->nvs_);
    this->nvs_ = 0;
    char namespace_name[16]{};
    snprintf(namespace_name, sizeof(namespace_name), "pp%08" PRIx32,
             this->counter_.device_key);
    err = nvs_open(namespace_name, NVS_READWRITE, &this->nvs_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Could not open per-meter NVS (%d)", err);
      this->nvs_ready_ = false;
      return;
    }
    size = sizeof(candidate);
    err = nvs_get_blob(this->nvs_, "checkpoint", &candidate, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGI(TAG, "New per-meter checkpoint will start at recent readings");
      return;
    }
  }
  if (err == ESP_OK && size == sizeof(candidate) &&
      candidate.device_key != this->counter_.device_key &&
      candidate.magic == accounting::CHECKPOINT_MAGIC &&
      candidate.version == accounting::CHECKPOINT_VERSION) {
    // Another meter already owns the original namespace. Do not overwrite it.
    // Use a distinct namespace for this meter while retaining old deployments.
    nvs_close(this->nvs_);
    this->nvs_ = 0;
    char namespace_name[16]{};
    snprintf(namespace_name, sizeof(namespace_name), "pp%08" PRIx32,
             this->counter_.device_key);
    err = nvs_open(namespace_name, NVS_READWRITE, &this->nvs_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Could not open per-meter NVS namespace (%d)", err);
      this->nvs_ready_ = false;
      return;
    }
    size = sizeof(candidate);
    err = nvs_get_blob(this->nvs_, "checkpoint", &candidate, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGI(TAG, "Independent NVS checkpoint for additional meter created");
      return;
    }
  }
  if (err != ESP_OK || size != sizeof(candidate) ||
      !accounting::compatible(candidate, this->counter_.device_key, this->pulses_per_kwh_)) {
    ESP_LOGE(TAG, "Existing checkpoint incompatible with this meter/calibration; refusing overwrite");
    this->nvs_ready_ = false;
    nvs_close(this->nvs_);
    this->nvs_ = 0;
    return;
  }
  this->counter_ = candidate;
  ESP_LOGI(TAG, "Recovered checkpoint: last=%" PRIu32 ", cumulative pulses=%" PRIu64,
           candidate.last_timestamp, candidate.total_pulses);
  this->publish_energy_(false);
}

void PalPulse::load_history_ack_() {
  this->history_ack_.device_key = this->counter_.device_key;
  this->history_ack_.timestamp = this->counter_.last_timestamp;
  this->history_ack_.total_pulses = this->counter_.total_pulses;
  if (!this->nvs_ready_) {
    this->history_replay_fault_ = true;
    return;
  }
  HistoryAck saved{};
  size_t bytes = sizeof(saved);
  const auto err = nvs_get_blob(this->nvs_, "history_ack_v2", &saved, &bytes);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    // For an existing v1 checkpoint, begin acknowledgments at the last
    // trusted counter position. Never erase or modify the v1 checkpoint.
    if (!this->save_history_ack_())
      this->history_replay_fault_ = true;
    return;
  }
  if (err != ESP_OK || bytes != sizeof(saved) ||
      saved.magic != this->history_ack_.magic || saved.version != 1 ||
      saved.device_key != this->counter_.device_key ||
      saved.timestamp > this->counter_.last_timestamp ||
      saved.total_pulses > this->counter_.total_pulses) {
    ESP_LOGE(TAG, "History acknowledgement cursor incompatible; replay paused");
    this->history_replay_fault_ = true;
    return;
  }
  this->history_ack_ = saved;
  ESP_LOGI(TAG, "Confirmed history cursor %" PRIu32, saved.timestamp);
}

bool PalPulse::save_history_ack_() {
  if (!this->nvs_ready_)
    return false;
  auto err = nvs_set_blob(this->nvs_, "history_ack_v2", &this->history_ack_, sizeof(this->history_ack_));
  if (err == ESP_OK)
    err = nvs_commit(this->nvs_);
  if (err != ESP_OK)
    ESP_LOGE(TAG, "Could not persist history acknowledgement (%d)", err);
  return err == ESP_OK;
}

void PalPulse::ack_history_(int32_t timestamp) {
  if (!this->reliable_history_enabled_ || timestamp <= 0 || this->history_count_ == 0)
    return;
  const auto record = this->history_queue_[this->history_head_];
  if (record.timestamp != static_cast<uint32_t>(timestamp))
    return;  // Never advance beyond an unconfirmed or out-of-order record.
  const auto previous = this->history_ack_;
  this->history_ack_.timestamp = record.timestamp;
  this->history_ack_.total_pulses = record.total_pulses;
  if (!this->save_history_ack_()) {
    this->history_ack_ = previous;
    return;
  }
  this->history_head_ = (this->history_head_ + 1) % HISTORY_QUEUE_SIZE;
  --this->history_count_;
  this->last_history_emit_ms_ = 0;
}

bool PalPulse::save_checkpoint_() {
  if (!this->checkpoint_dirty_)
    return this->nvs_ready_;
  if (!this->nvs_ready_)
    return false;
  auto err = nvs_set_blob(this->nvs_, "checkpoint", &this->counter_, sizeof(this->counter_));
  if (err == ESP_OK)
    err = nvs_commit(this->nvs_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS checkpoint commit failed (%d)", err);
    return false;
  }
  this->checkpoint_dirty_ = false;
  return true;
}

void PalPulse::publish_energy_(bool caught_up) {
  if (this->energy_)
    this->energy_->publish_state(static_cast<float>(this->counter_.total_pulses) / this->pulses_per_kwh_);
  if (!caught_up || !this->daily_)
    return;
#ifdef USE_TIME
  if (!this->clock_)
    return;
  const auto now = this->clock_->now();
  if (!now.is_valid() || this->counter_.day_key !=
         uint32_t(now.year) * 1000 + now.day_of_year)
    return;
  this->daily_->publish_state(static_cast<float>(this->counter_.daily_pulses) / this->pulses_per_kwh_);
#endif
}

uint32_t PalPulse::local_day_(uint32_t timestamp) const {
#ifdef USE_TIME
  if (this->clock_ && this->clock_->now().is_valid()) {
    auto recorded = ESPTime::from_epoch_local(timestamp);
    if (recorded.is_valid())
      return uint32_t(recorded.year) * 1000 + recorded.day_of_year;
  }
#endif
  return 0;  // No reliable timezone: never reset the daily counter on a guess.
}

void PalPulse::clear_connection_() {
  this->cancel_timeout("palpulse_poll");
  this->cancel_timeout("palpulse_timeout");
  this->cancel_timeout("palpulse_subscribe_retry");
  if (this->checkpoint_dirty_)
    this->save_checkpoint_();
  if (this->power_)
    this->power_->publish_state(NAN);  // Never leave a stale live watt value current.
  this->attr_ = {};
  this->phase_ = Phase::OFFLINE;
  this->request_ = Request::NONE;
  this->live_subscribed_ = false;
  this->clock_set_ = false;
  this->pairing_pending_ = false;
  this->initialized_ = false;
  this->first_record_ = 0;
  this->newest_record_ = 0;
  this->active_start_ = 0;
  this->active_end_ = 0;
  this->records_seen_ = 0;
}

bool PalPulse::discover_() {
  using espbt::ESPBTUUID;
  const auto service = ESPBTUUID::from_raw(POWERPAL_BASE);
  auto lookup = [&](uint16_t slot) -> uint16_t {
    auto *character = this->parent()->get_characteristic(service, characteristic(slot));
    return character ? character->handle : 0;
  };
  this->attr_.pairing = lookup(0x0011);
  this->attr_.interval = lookup(0x0013);
  this->attr_.clock = lookup(0x0004);
  this->attr_.bounds = lookup(0x0005);
  this->attr_.request = lookup(0x0002);
  this->attr_.records = lookup(0x0001);
  this->attr_.live = lookup(0x0003);
  if (this->battery_) {
    auto *battery = this->parent()->get_characteristic(ESPBTUUID::from_uint16(0x180F),
                                                        ESPBTUUID::from_uint16(0x2A19));
    this->attr_.battery = battery ? battery->handle : 0;
  }
  if (!this->attr_.pairing || !this->attr_.interval || !this->attr_.bounds ||
      !this->attr_.request || !this->attr_.records) {
    ESP_LOGE(TAG, "Powerpal BLE service is missing one or more required characteristics");
    return false;
  }
  return true;
}

bool PalPulse::write_characteristic_(uint16_t handle, uint8_t *buffer, uint16_t length) {
  if (!handle)
    return false;
  auto result = esp_ble_gattc_write_char(this->parent()->get_gattc_if(),
                                         this->parent()->get_conn_id(), handle, length, buffer,
                                         ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (result != ESP_OK)
    ESP_LOGW(TAG, "Failed to queue BLE write to handle 0x%04x: %d", handle, result);
  return result == ESP_OK;
}

void PalPulse::begin_pairing_() {
  if (this->phase_ != Phase::PAIRING || this->pairing_pending_)
    return;
  uint8_t bytes[4];
  wire::put_u32le(bytes, this->pairing_code_);
  if (this->write_characteristic_(this->attr_.pairing, bytes, sizeof(bytes))) {
    this->pairing_pending_ = true;
  } else {
    this->set_timeout("palpulse_subscribe_retry", 5000, [this]() { this->begin_pairing_(); });
  }
}

bool PalPulse::write_clock_if_valid_() {
#ifdef USE_TIME
  if (this->clock_set_ || !this->attr_.clock || !this->clock_)
    return false;
  auto now = this->clock_->now();
  if (!now.is_valid())
    return false;
  uint8_t bytes[4];
  wire::put_u32le(bytes, static_cast<uint32_t>(now.timestamp));
  if (!this->write_characteristic_(this->attr_.clock, bytes, sizeof(bytes)))
    return false;
  this->phase_ = Phase::CLOCK;
  return true;
#else
  return false;
#endif
}

void PalPulse::configure_interval_() {
  this->phase_ = Phase::INTERVAL;
  uint8_t bytes[4]{};
  wire::interval_payload(bytes, this->interval_minutes_);
  if (!this->write_characteristic_(this->attr_.interval, bytes, sizeof(bytes)))
    this->set_timeout("palpulse_subscribe_retry", 5000, [this]() { this->configure_interval_(); });
}

void PalPulse::subscribe_records_() {
  this->phase_ = Phase::SUBSCRIBING;
  const auto status = this->parent()->register_for_notify(this->attr_.records);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "Cannot subscribe to Powerpal historical records: %d", status);
    this->set_timeout("palpulse_subscribe_retry", 5000, [this]() { this->subscribe_records_(); });
  }
}

void PalPulse::finish_subscription_() {
  this->node_state = espbt::ClientState::ESTABLISHED;
  this->phase_ = Phase::READY;
  this->initialized_ = true;
  if (this->attr_.battery) {
    auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(),
                                           this->parent()->get_conn_id(), this->attr_.battery,
                                           ESP_GATT_AUTH_REQ_NONE);
    if (status != ESP_OK)
      ESP_LOGW(TAG, "Battery read could not be queued: %d", status);
  }
  this->schedule_poll_(1500);
}

void PalPulse::schedule_poll_(uint32_t milliseconds) {
  this->set_timeout("palpulse_poll", milliseconds, [this]() { this->poll_(); });
}

void PalPulse::poll_() {
  if (this->phase_ != Phase::READY || this->request_ != Request::NONE)
    return;
  if (this->write_clock_if_valid_())
    return;
  if (this->reliable_history_enabled_ && this->history_count_ != 0) {
    // Do not poll BLE for another batch while the native API is awaiting
    // durable Home Assistant ACKs. Loop() continues transmitting the head.
    this->schedule_poll_(1000);
    return;
  }
  this->read_bounds_();
}

void PalPulse::read_bounds_() {
  if (!this->attr_.bounds)
    return;
  this->request_ = Request::BOUNDS;
  const auto result = esp_ble_gattc_read_char(this->parent()->get_gattc_if(),
                                               this->parent()->get_conn_id(), this->attr_.bounds,
                                               ESP_GATT_AUTH_REQ_NONE);
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "Powerpal bounds read failed to queue: %d", result);
    this->request_ = Request::NONE;
    this->schedule_poll_(5000);
    return;
  }
  this->set_timeout("palpulse_timeout", 15000, [this]() { this->request_timeout_(); });
}

void PalPulse::handle_bounds_(const uint8_t *bytes, size_t length) {
  this->cancel_timeout("palpulse_timeout");
  this->request_ = Request::NONE;
  wire::Range bounds{};
  if (!wire::range(bytes, length, bounds)) {
    ESP_LOGW(TAG, "Powerpal returned invalid buffered measurement bounds");
    this->schedule_poll_(5000);
    return;
  }
  if (this->reliable_history_enabled_) {
    if (this->history_replay_fault_)
      return;
    if (this->history_count_) {
      this->schedule_poll_(1000);
      return; // Drain acknowledged history before requesting another batch.
    }
    if (this->history_ack_.timestamp &&
        bounds.oldest > this->history_ack_.timestamp + 60) {
      ESP_LOGE(TAG, "Meter buffer lost unacknowledged records; replay paused");
      this->history_replay_fault_ = true;
      return;
    }
    this->history_replay_total_ = this->history_ack_.total_pulses;
  }
  const uint32_t cursor = this->reliable_history_enabled_
      ? this->history_ack_.timestamp : this->counter_.last_timestamp;
  const auto window = accounting::next_window(bounds.oldest, bounds.newest,
                                               cursor,
                                               uint32_t(this->interval_minutes_) * 60);
  if (!window.valid) {
    this->schedule_poll_(uint32_t(this->interval_minutes_) * 60000);
    return;
  }
  if (this->counter_.last_timestamp && this->counter_.last_timestamp < bounds.oldest)
    ESP_LOGW(TAG, "Meter buffer has rolled past checkpoint cursor; older records may be unrecoverable");

  this->first_record_ = bounds.oldest;
  this->newest_record_ = bounds.newest;
  this->active_start_ = window.start;
  this->active_end_ = window.end;
  this->records_seen_ = 0;
  uint8_t payload[8];
  wire::request_payload(payload, window.start, window.end);
  this->request_ = Request::STREAM;
  if (!this->write_characteristic_(this->attr_.request, payload, sizeof(payload))) {
    this->request_ = Request::NONE;
    this->schedule_poll_(5000);
    return;
  }
  ESP_LOGD(TAG, "Requested records %" PRIu32 " to %" PRIu32, window.start, window.end);
  this->set_timeout("palpulse_timeout", 45000, [this]() { this->request_timeout_(); });
}

void PalPulse::handle_record_(const uint8_t *bytes, size_t length) {
  if (this->request_ != Request::STREAM)
    return;
  wire::Sample input{};
  if (!wire::sample(bytes, length, input)) {
    ESP_LOGW(TAG, "Dropped truncated measurement packet (%u bytes)", static_cast<unsigned>(length));
    return;
  }
  if (input.recorded_at < this->active_start_ || input.recorded_at > this->active_end_ ||
      !accounting::on_record_phase(input.recorded_at, this->first_record_)) {
    ESP_LOGW(TAG, "Dropped non-native or out-of-window record at %" PRIu32, input.recorded_at);
    return;
  }
  if (this->reliable_history_enabled_ && this->history_replay_fault_)
    return;
  if (this->reliable_history_enabled_ &&
      input.recorded_at <= this->counter_.last_timestamp) {
    if (input.recorded_at <= this->history_ack_.timestamp)
      return;
    this->history_replay_total_ += input.pulse_count;
    if (input.recorded_at == this->counter_.last_timestamp &&
        this->history_replay_total_ != this->counter_.total_pulses) {
      ESP_LOGE(TAG, "Replay meter-counter mismatch; stopped rather than inventing energy");
      this->history_replay_fault_ = true;
      return;
    }
    this->queue_history_record_(
        input.recorded_at, input.pulse_count, this->history_replay_total_);
    ++this->records_seen_;
    if (input.recorded_at == this->active_end_)
      this->stream_complete_();
    return;
  }
  if (this->reliable_history_enabled_ &&
      this->history_replay_total_ != this->counter_.total_pulses) {
    ESP_LOGE(TAG, "Replay cursor does not match live counter; history paused");
    this->history_replay_fault_ = true;
    return;
  }
  const auto power = accounting::measure_power(input.recorded_at, this->counter_.last_timestamp,
                                                input.pulse_count, this->pulses_per_kwh_);
  if (!power.is_new)
    return;
  const bool same_phase = !this->counter_.last_timestamp ||
      input.recorded_at % accounting::RECORD_PERIOD_SECONDS ==
      this->counter_.last_timestamp % accounting::RECORD_PERIOD_SECONDS;
  if (!accounting::apply(this->counter_, input.recorded_at, input.pulse_count,
                          this->local_day_(input.recorded_at)))
    return;
  if (this->reliable_history_enabled_)
    this->history_replay_total_ += input.pulse_count;
  this->checkpoint_dirty_ = true;
  this->queue_history_record_(input.recorded_at, input.pulse_count, this->counter_.total_pulses);
  ++this->records_seen_;
  if (!this->live_subscribed_ && power.has_value && same_phase && this->power_)
    this->power_->publish_state(power.watts);
  if (input.recorded_at == this->active_end_)
    this->stream_complete_();
}

void PalPulse::handle_live_(const uint8_t *bytes, size_t length) {
  if (!this->live_subscribed_ || !this->power_)
    return;
  uint32_t millis_per_pulse = 0;
  if (!wire::pulse_interval_ms(bytes, length, millis_per_pulse))
    return;
  this->power_->publish_state(3600000000.0f /
                             (this->pulses_per_kwh_ * float(millis_per_pulse)));
}

void PalPulse::stream_complete_() {
  this->cancel_timeout("palpulse_timeout");
  this->request_ = Request::NONE;
  if (!this->save_checkpoint_()) {
    ESP_LOGE(TAG, "Suppressing cumulative energy publish because checkpoint is not durable");
    this->schedule_poll_(5000);
    return;
  }
  this->publish_energy_(this->counter_.last_timestamp >= this->newest_record_);
  if (this->attr_.battery) {
    const auto err = esp_ble_gattc_read_char(this->parent()->get_gattc_if(),
                                              this->parent()->get_conn_id(), this->attr_.battery,
                                              ESP_GATT_AUTH_REQ_NONE);
    if (err != ESP_OK)
      ESP_LOGD(TAG, "Battery refresh was not queued: %d", err);
  }
  // The energy counter can be caught up while the durable Home Assistant
  // ACK cursor is many buffered batches behind. Continue replay immediately;
  // otherwise 30-minute records per five minutes cannot keep up with a
  // meter generating 60 records per hour during an outage.
  const bool history_behind = this->reliable_history_enabled_ &&
                              this->history_events_enabled_ &&
                              this->history_ack_.timestamp < this->newest_record_;
  this->schedule_poll_(
      this->counter_.last_timestamp < this->newest_record_ || history_behind
          ? 1000
          : uint32_t(this->interval_minutes_) * 60000);
}

void PalPulse::request_timeout_() {
  ESP_LOGW(TAG, "Powerpal request timed out; retrying from last durable or in-memory timestamp");
  if (this->request_ == Request::STREAM && this->checkpoint_dirty_) {
    if (this->save_checkpoint_())
      this->publish_energy_(false);
  }
  this->request_ = Request::NONE;
  this->schedule_poll_(5000);
}

void PalPulse::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t interface,
                                   esp_ble_gattc_cb_param_t *params) {
  (void) interface;
  // Disconnect events can arrive without a parameter block. Always clear
  // outstanding timers and stale connection handles on either lifecycle event.
  if (event == ESP_GATTC_DISCONNECT_EVT || event == ESP_GATTC_CLOSE_EVT) {
    this->clear_connection_();
    return;
  }
  if (!params)
    return;
  switch (event) {
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
      if (!this->discover_()) {
        this->status_set_warning();
        break;
      }
      this->phase_ = Phase::PAIRING;
      this->begin_pairing_();
      break;
    case ESP_GATTC_WRITE_CHAR_EVT: {
      const auto &write = params->write;
      if (write.handle == this->attr_.pairing && this->phase_ == Phase::PAIRING) {
        this->pairing_pending_ = false;
        if (write.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Pairing-code write rejected: %d", write.status);
          this->set_timeout("palpulse_subscribe_retry", 5000, [this]() { this->begin_pairing_(); });
        } else if (!this->write_clock_if_valid_()) {
          this->configure_interval_();
        }
      } else if (write.handle == this->attr_.clock && this->phase_ == Phase::CLOCK) {
        this->clock_set_ = write.status == ESP_GATT_OK;
        if (!this->initialized_)
          this->configure_interval_();
        else {
          this->phase_ = Phase::READY;
          this->read_bounds_();
        }
      } else if (write.handle == this->attr_.interval && this->phase_ == Phase::INTERVAL) {
        if (write.status == ESP_GATT_OK)
          this->subscribe_records_();
        else
          this->set_timeout("palpulse_subscribe_retry", 5000, [this]() { this->configure_interval_(); });
      } else if (write.handle == this->attr_.request && this->request_ == Request::STREAM &&
                 write.status != ESP_GATT_OK) {
        this->cancel_timeout("palpulse_timeout");
        this->request_ = Request::NONE;
        this->schedule_poll_(5000);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (params->reg_for_notify.handle == this->attr_.records &&
          this->phase_ == Phase::SUBSCRIBING) {
        if (params->reg_for_notify.status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Measurement notification registration failed: %d",
                   params->reg_for_notify.status);
          this->set_timeout("palpulse_subscribe_retry", 5000, [this]() { this->subscribe_records_(); });
        } else if (this->wants_live_power_ && this->attr_.live) {
          const auto err = this->parent()->register_for_notify(this->attr_.live);
          if (err != ESP_OK) {
            ESP_LOGW(TAG, "Live power unavailable, using interval averages");
            this->finish_subscription_();
          }
        } else {
          this->finish_subscription_();
        }
      } else if (params->reg_for_notify.handle == this->attr_.live &&
                 this->phase_ == Phase::SUBSCRIBING) {
        this->live_subscribed_ = params->reg_for_notify.status == ESP_GATT_OK;
        if (!this->live_subscribed_)
          ESP_LOGW(TAG, "Live pulse subscription failed; using historical averages");
        this->finish_subscription_();
      }
      break;
    case ESP_GATTC_READ_CHAR_EVT:
      if (params->read.handle == this->attr_.bounds && this->request_ == Request::BOUNDS) {
        if (params->read.status == ESP_GATT_OK)
          this->handle_bounds_(params->read.value, params->read.value_len);
        else {
          this->cancel_timeout("palpulse_timeout");
          this->request_ = Request::NONE;
          this->schedule_poll_(5000);
        }
      } else if (params->read.handle == this->attr_.battery &&
                 params->read.status == ESP_GATT_OK && this->battery_) {
        uint8_t level = 0;
        if (wire::battery_percent(params->read.value, params->read.value_len, level))
          this->battery_->publish_state(level);
      }
      break;
    case ESP_GATTC_NOTIFY_EVT:
      if (params->notify.handle == this->attr_.records)
        this->handle_record_(params->notify.value, params->notify.value_len);
      else if (params->notify.handle == this->attr_.live)
        this->handle_live_(params->notify.value, params->notify.value_len);
      else if (params->notify.handle == this->attr_.battery && this->battery_) {
        uint8_t level = 0;
        if (wire::battery_percent(params->notify.value, params->notify.value_len, level))
          this->battery_->publish_state(level);
      }
      break;
    default:
      break;
  }
}

void PalPulse::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *params) {
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT || !params)
    return;
  if (params->ble_security.auth_cmpl.success && this->phase_ == Phase::PAIRING)
    this->begin_pairing_();
}

}  // namespace esphome::powerpal_ble
#endif
