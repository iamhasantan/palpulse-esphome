#pragma once

// Layout-compatible with the previously deployed v1 checkpoint.
// Measurement and checkpoint helper algorithms were originally authored
// during an earlier in-house prototype. See docs/PROVENANCE.md for audit
// links. Do not change the serialized layout without migration tests.
#include <cstdint>
#include <limits>

namespace esphome::powerpal_ble::accounting {

constexpr uint32_t CHECKPOINT_MAGIC = 0x46505031u;  // FPP1
constexpr uint32_t CHECKPOINT_VERSION = 1;
constexpr uint32_t RECORD_PERIOD_SECONDS = 60;
constexpr uint32_t MAX_RECORDS_PER_REQUEST = 30;

struct Checkpoint {
  uint32_t magic{CHECKPOINT_MAGIC};
  uint32_t version{CHECKPOINT_VERSION};
  uint32_t device_key{0};
  uint32_t last_timestamp{0};
  uint32_t day_key{0};
  float pulses_per_kwh{0.0f};
  uint64_t total_pulses{0};
  uint64_t daily_pulses{0};
};

static_assert(sizeof(Checkpoint) == 40, "Legacy checkpoint layout changed");

inline uint32_t meter_identity_hash(const char *mac) {
  // Standard FNV-1a 32-bit. Required for existing NVS identity compatibility.
  uint32_t hash = 2166136261u;
  if (mac) {
    for (const auto *p = reinterpret_cast<const unsigned char *>(mac); *p; ++p) {
      hash ^= *p;
      hash *= 16777619u;
    }
  }
  return hash;
}

inline bool compatible(const Checkpoint &value, uint32_t device_key, float calibration) {
  return value.magic == CHECKPOINT_MAGIC && value.version == CHECKPOINT_VERSION &&
         value.device_key == device_key && calibration > 0 && value.pulses_per_kwh == calibration &&
         value.daily_pulses <= value.total_pulses &&
         (value.last_timestamp != 0 || value.total_pulses == 0);
}

struct MeanPower {
  bool is_new{false};
  bool has_value{false};
  uint32_t seconds{0};
  float watts{0.0f};
};

inline MeanPower measure_power(uint32_t timestamp, uint32_t previous, uint16_t pulses, float calibration) {
  MeanPower result{};
  if (!timestamp || (previous && timestamp <= previous))
    return result;
  result.is_new = true;
  if (!previous || calibration <= 0)
    return result;
  result.seconds = timestamp - previous;
  if (result.seconds < 30 || result.seconds > 300)
    return result;
  result.has_value = true;
  result.watts = (static_cast<float>(pulses) * 3600000.0f) /
                 (calibration * static_cast<float>(result.seconds));
  return result;
}

inline bool apply(Checkpoint &state, uint32_t timestamp, uint16_t pulses, uint32_t local_day) {
  if (!timestamp || (state.last_timestamp && timestamp <= state.last_timestamp))
    return false;
  if (local_day && state.day_key != local_day) {
    state.day_key = local_day;
    state.daily_pulses = 0;
  }
  state.total_pulses += pulses;
  state.daily_pulses += pulses;
  state.last_timestamp = timestamp;
  return true;
}

struct Window {
  uint32_t start{0};
  uint32_t end{0};
  bool valid{false};
};

inline Window next_window(uint32_t first, uint32_t last, uint32_t cursor, uint32_t batch_seconds) {
  if (!first || last < first)
    return {};
  uint64_t start;
  if (!cursor) {
    const uint32_t recent = last > batch_seconds ? last - batch_seconds : 0;
    start = recent > first ? recent : first;
    // First and last records are assumed to share a 60-second phase.
    start = first + ((start - first + RECORD_PERIOD_SECONDS - 1) / RECORD_PERIOD_SECONDS) *
                    RECORD_PERIOD_SECONDS;
  } else {
    if (cursor >= last)
      return {};
    start = cursor < first ? first :
            uint64_t(first) + ((uint64_t(cursor) - first) / RECORD_PERIOD_SECONDS + 1) *
                                  RECORD_PERIOD_SECONDS;
  }
  if (start > last || start > std::numeric_limits<uint32_t>::max())
    return {};
  const uint64_t last_in_window = start + (MAX_RECORDS_PER_REQUEST - 1) * RECORD_PERIOD_SECONDS;
  return {static_cast<uint32_t>(start),
          static_cast<uint32_t>(last_in_window < last ? last_in_window : last), true};
}

inline bool on_record_phase(uint32_t timestamp, uint32_t first) {
  return timestamp && first && timestamp % RECORD_PERIOD_SECONDS == first % RECORD_PERIOD_SECONDS;
}

}  // namespace esphome::powerpal_ble::accounting
