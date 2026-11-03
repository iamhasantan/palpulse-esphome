#pragma once

// PalPulse wire format. The UUIDs and packet formats are protocol facts.
// No legacy Powerpal integration implementation is imported into this module.
#include <cstddef>
#include <cstdint>

namespace esphome::powerpal_ble::wire {

struct Sample {
  uint32_t recorded_at{0};
  uint16_t pulse_count{0};
};

struct Range {
  uint32_t oldest{0};
  uint32_t newest{0};
};

inline uint32_t u32le(const uint8_t *bytes) {
  return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
         (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

inline void put_u32le(uint8_t *bytes, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[shift / 8] = static_cast<uint8_t>(value >> shift);
}

inline bool sample(const uint8_t *bytes, size_t size, Sample &out) {
  if (!bytes || size < 6)
    return false;
  out.recorded_at = u32le(bytes);
  out.pulse_count = uint16_t(bytes[4]) | (uint16_t(bytes[5]) << 8);
  return true;
}

inline bool range(const uint8_t *bytes, size_t size, Range &out) {
  if (!bytes || size < 8)
    return false;
  out.oldest = u32le(bytes);
  out.newest = u32le(bytes + 4);
  return out.oldest != 0 && out.newest >= out.oldest;
}

inline bool pulse_interval_ms(const uint8_t *bytes, size_t size, uint32_t &out) {
  if (!bytes || size < 4)
    return false;
  out = u32le(bytes);
  return out != 0;
}

inline bool battery_percent(const uint8_t *bytes, size_t size, uint8_t &out) {
  if (!bytes || size != 1 || bytes[0] > 100)
    return false;
  out = bytes[0];
  return true;
}

// The Powerpal batch-size GATT characteristic accepts a 32-bit little-endian
// value in minutes, not a single-byte value. Preserve that protocol encoding.
inline void interval_payload(uint8_t (&bytes)[4], uint32_t minutes) {
  put_u32le(bytes, minutes);
}

inline void request_payload(uint8_t (&bytes)[8], uint32_t start, uint32_t end) {
  put_u32le(bytes, start);
  put_u32le(bytes + 4, end);
}

}  // namespace esphome::powerpal_ble::wire
