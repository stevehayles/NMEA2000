#include "NMEA2000.h"
#include "N2kTimer.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <vector>

namespace {
uint64_t now_ms;

struct Frame {
  unsigned long id;
  unsigned char size;
  std::array<unsigned char, 8> data;
};

class TestBus : public tNMEA2000 {
public:
  std::deque<Frame> incoming;
  std::vector<Frame> outgoing;

protected:
  bool CANOpen() override { return true; }
  bool CANSendFrame(unsigned long id, unsigned char size, const unsigned char *data,
                    bool) override {
    Frame frame{id, size, {{0}}};
    for (unsigned i = 0; i < size; ++i) frame.data[i] = data[i];
    outgoing.push_back(frame);
    return true;
  }
  bool CANGetFrame(unsigned long &id, unsigned char &size, unsigned char *data) override {
    if (incoming.empty()) return false;
    const Frame frame = incoming.front();
    incoming.pop_front();
    id = frame.id;
    size = frame.size;
    for (unsigned i = 0; i < size; ++i) data[i] = frame.data[i];
    return true;
  }
};
} // namespace

uint64_t tN2kSyncScheduler::SyncOffset = 0;
uint64_t N2kMillis64() { return now_ms; }
uint32_t N2kMillis() { return static_cast<uint32_t>(now_ms); }
extern "C" int64_t esp_timer_get_time() { return static_cast<int64_t>(now_ms * 1000); }
extern "C" bool rtc_try_get_timeofday(double *) { return false; }

int main() {
  TestBus bus;
  bus.SetMode(tNMEA2000::N2km_ListenAndNode, 0x1e);
  for (unsigned i = 0; i < 15 && !bus.IsOpen(); ++i) {
    bus.ParseMessages();
    now_ms += 30;
  }
  assert(bus.IsOpen());
  now_ms += 260;
  bus.ParseMessages();
  bus.outgoing.clear();

  // A PGN 59904 payload with an invalid 18-bit requested PGN is not an ISO request.
  bus.incoming.push_back({0x18ea1e21UL, 8, {{0xff, 0xff, 0xff, 4, 5, 6, 7, 8}}});
  bus.ParseMessages();
  assert(bus.outgoing.empty());

  // A normal three-byte ISO request still gets an address-claim response.
  bus.incoming.push_back({0x18ea1e21UL, 3, {{0x00, 0xee, 0x00, 0, 0, 0, 0, 0}}});
  bus.ParseMessages();
  assert(!bus.outgoing.empty());
  assert(bus.outgoing.back().id == 0x18eeff1eUL);

  // Malformed claims cannot displace our address or provoke a response.
  bus.outgoing.clear();
  bus.incoming.push_back({0x18eeff1eUL, 8, {{0, 0, 0, 0, 0, 0, 0, 0}}});
  bus.incoming.push_back({0x18eeff1eUL, 7, {{1, 0, 0, 0, 0, 0, 0, 0}}});
  bus.incoming.push_back({0x18ee211eUL, 8, {{1, 0, 0, 0, 0, 0, 0, 0}}});
  bus.ParseMessages();
  assert(bus.GetN2kSource() == 0x1e && bus.outgoing.empty());
}
