#include "../components/powerpal_ble/wire.h"
#include <cassert>
#include <cstdint>

using namespace esphome::powerpal_ble;

int main() {
  const uint8_t one[] = {0x04, 0x03, 0x02, 0x01, 0x34, 0x12};
  wire::Sample s;
  assert(!wire::sample(nullptr, 6, s));
  assert(!wire::sample(one, 5, s));
  assert(wire::sample(one, 6, s));
  assert(s.recorded_at == 0x01020304u && s.pulse_count == 0x1234u);
  uint8_t encoded_interval[4]{};
  wire::interval_payload(encoded_interval, 5);
  assert(encoded_interval[0] == 5 && encoded_interval[1] == 0 &&
         encoded_interval[2] == 0 && encoded_interval[3] == 0);
  wire::interval_payload(encoded_interval, 60);
  assert(wire::u32le(encoded_interval) == 60);
  uint8_t request[8]{};
  wire::request_payload(request, 0x12345678u, 0x9ABCDEF0u);
  assert(request[0] == 0x78 && request[3] == 0x12 && request[4] == 0xF0 && request[7] == 0x9A);
  wire::Range r;
  assert(wire::range(request, 8, r));
  assert(r.oldest == 0x12345678u && r.newest == 0x9ABCDEF0u);
  assert(!wire::range(request, 7, r));
  assert(!wire::battery_percent(nullptr, 1, request[0]));
  const uint8_t high[] = {101};
  assert(!wire::battery_percent(high, 1, request[0]));
  uint32_t ms = 0;
  assert(wire::pulse_interval_ms(one, 4, ms) && ms == 0x01020304u);
  const uint8_t zero[] = {0, 0, 0, 0};
  assert(!wire::pulse_interval_ms(zero, 4, ms));
}
