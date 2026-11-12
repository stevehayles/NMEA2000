#include "NMEA2000.h"
#include "N2kTimer.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <deque>

namespace {
uint64_t now_ms;
unsigned received_wind;

struct Frame {
  unsigned long id;
  std::array<unsigned char, 8> data;
};

class TestBus : public tNMEA2000 {
public:
  std::deque<Frame> incoming;

protected:
  bool CANOpen() override { return true; }
  bool CANSendFrame(unsigned long, unsigned char, const unsigned char *, bool) override {
    return true;
  }
  bool CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *data) override {
    if (incoming.empty()) return false;
    const auto frame = incoming.front();
    incoming.pop_front();
    id = frame.id;
    len = 8;
    for (unsigned i = 0; i < 8; ++i) data[i] = frame.data[i];
    return true;
  }
};

void collect(const tN2kMsg &message) {
  if (message.PGN == 130306) ++received_wind;
}

bool delivered_after(uint64_t start, uint64_t quiet, bool poll_quiet_bus = true) {
  now_ms = start - 300;
  received_wind = 0;
  TestBus bus;
  const unsigned long fast_packets[] = {129029, 0};
  bus.SetMode(tNMEA2000::N2km_ListenOnly);
  bus.SetN2kCANMsgBufSize(2);
  bus.ExtendFastPacketMessages(fast_packets);
  bus.SetMsgHandler(collect);
  for (unsigned i = 0; i < 20 && !bus.IsOpen(); ++i) {
    bus.ParseMessages();
    now_ms += 30;
  }
  assert(bus.IsOpen());

  now_ms = start;
  bus.incoming.push_back({0x19f80501UL, {{0, 16, 1, 2, 3, 4, 5, 6}}});
  bus.incoming.push_back({0x19f80502UL, {{0, 16, 1, 2, 3, 4, 5, 6}}});
  bus.ParseMessages();
  assert(received_wind == 0);

  if (poll_quiet_bus) {
    for (uint64_t elapsed = 600000; elapsed < quiet; elapsed += 600000) {
      now_ms = start + elapsed;
      bus.ParseMessages();
    }
  }
  now_ms = start + quiet;
  bus.incoming.push_back({0x19fd0214UL, {{1, 0, 0, 0, 0, 0, 0, 0}}});
  bus.ParseMessages();
  return received_wind == 1;
}
} // namespace

uint64_t tN2kSyncScheduler::SyncOffset = 0;
uint64_t N2kMillis64() { return now_ms; }
uint32_t N2kMillis() { return static_cast<uint32_t>(now_ms); }
extern "C" int64_t esp_timer_get_time() { return static_cast<int64_t>(now_ms * 1000); }
extern "C" bool rtc_try_get_timeofday(double *) { return false; }

int main() {
  static_assert(sizeof(tN2kSchedulerTime) == 8, "Linux uses the 64-bit scheduler");
  constexpr uint64_t wrap = uint64_t{1} << 32;
  for (const uint64_t start : {uint64_t{1000}, wrap / 2 - 50, wrap - 50}) {
    assert(!delivered_after(start, 99));
    assert(delivered_after(start, 100));
    assert(delivered_after(start, wrap / 2 + 1000, false));
    assert(delivered_after(start, wrap + 50));
    assert(delivered_after(start, wrap + 100));
  }
}
