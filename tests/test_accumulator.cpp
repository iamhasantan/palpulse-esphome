#include "../components/powerpal_ble/accumulator.h"
#include <cassert>
#include <cmath>
#include <cstring>

using namespace esphome::powerpal_ble::accounting;

int main() {
  Checkpoint s{};
  s.device_key = legacy_device_key("AA:BB:CC:DD:EE:FF");
  s.pulses_per_kwh = 3200.0f;
  assert(compatible(s, s.device_key, 3200.0f));
  assert(!compatible(s, s.device_key, 1000.0f));
  assert(apply(s, 120, 50, 2026308));
  assert(!apply(s, 120, 50, 2026308));
  assert(s.total_pulses == 50 && s.daily_pulses == 50);
  assert(apply(s, 180, 80, 2026308));
  assert(s.total_pulses == 130 && s.daily_pulses == 130);
  assert(apply(s, 240, 10, 2026309));
  assert(s.total_pulses == 140 && s.daily_pulses == 10);
  auto p = measure_power(300, 240, 32, 3200.0f);
  assert(p.is_new && p.has_value && std::fabs(p.watts - 600.0f) < 0.01f);
  assert(!measure_power(240, 240, 32, 3200.0f).is_new);
  assert(!measure_power(900, 240, 32, 3200.0f).has_value);
  auto w = next_window(1000, 1000 + 90 * 60, 0, 300);
  assert(w.valid && w.start == 1000 + 5 * 60 && w.end == 1000 + 34 * 60);
  w = next_window(1000, 1000 + 90 * 60, 1000 + 10 * 60, 300);
  assert(w.start == 1000 + 11 * 60 && w.end == 1000 + 40 * 60);
  assert(!next_window(1000, 1500, 1500, 300).valid);
  assert(on_record_phase(1060, 1000) && !on_record_phase(1061, 1000));
  // A byte-for-byte save/restore does not alter the v1 structure.
  Checkpoint restored;
  std::memcpy(&restored, &s, sizeof(s));
  assert(compatible(restored, s.device_key, 3200.0f));
  assert(restored.total_pulses == 140);
}
