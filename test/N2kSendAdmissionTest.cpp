#include "NMEA2000.h"
#include "N2kTimer.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {
uint64_t now_ms;

struct Frame {
  unsigned long id;
  unsigned char size;
  unsigned char first;
  bool waited;
};

class TestBus : public tNMEA2000 {
public:
  bool blocked = false;
  std::vector<Frame> sent;

protected:
  bool CANOpen() override { return true; }
  bool CANSendFrame(unsigned long id, unsigned char size, const unsigned char *data,
                    bool wait_sent) override {
    if (blocked) return false;
    sent.push_back({id, size, size ? data[0] : static_cast<unsigned char>(0), wait_sent});
    return true;
  }
  bool CANGetFrame(unsigned long &, unsigned char &, unsigned char *) override { return false; }
};

void open(TestBus &bus) {
  bus.SetMode(tNMEA2000::N2km_ListenAndNode, 0x1e);
  bus.SetN2kCANSendFrameBufSize(4); // Three usable slots in the library ring.
  for (unsigned i = 0; i < 15 && !bus.IsOpen(); ++i) {
    bus.ParseMessages();
    now_ms += 30;
  }
  assert(bus.IsOpen());
  now_ms += 260;
  bus.ParseMessages();
  bus.sent.clear();
}

tN2kMsg message(unsigned long pgn, unsigned length) {
  tN2kMsg msg;
  msg.SetPGN(pgn);
  for (unsigned i = 0; i < length; ++i) msg.AddByte(static_cast<unsigned char>(i + 1));
  return msg;
}
} // namespace

uint64_t tN2kSyncScheduler::SyncOffset = 0;
uint64_t N2kMillis64() { return now_ms; }
uint32_t N2kMillis() { return static_cast<uint32_t>(now_ms); }
extern "C" int64_t esp_timer_get_time() { return static_cast<int64_t>(now_ms * 1000); }
extern "C" bool rtc_try_get_timeofday(double *) { return false; }

int main() {
  const tN2kMsg single = message(130306, 8);
  const tN2kMsg fast = message(129029, 16);

  // A queued message must precede a direct driver offer, with no hidden retry.
  TestBus bus;
  open(bus);
  bus.blocked = true;
  assert(bus.SendMsgNonBlocking(single));
  assert(bus.sent.empty());
  bus.blocked = false;
  assert(!bus.SendSingleFrameToDriver(single));
  assert(bus.sent.empty());
  bus.ParseMessages();
  assert(bus.sent.size() == 1 && !bus.sent[0].waited);
  assert(bus.SendSingleFrameToDriver(single));
  assert(bus.sent.size() == 2 && !bus.sent[1].waited);
  assert(!bus.SendSingleFrameToDriver(fast));
  assert(bus.sent.size() == 2);
  bus.blocked = true;
  assert(!bus.SendSingleFrameToDriver(single));
  bus.blocked = false;
  bus.ParseMessages();
  assert(bus.sent.size() == 2);

  // A failed fast-packet admission emits no frames and consumes no sequence.
  TestBus saturated;
  open(saturated);
  saturated.blocked = true;
  assert(saturated.SendMsgNonBlocking(single));
  assert(!saturated.SendMsgNonBlocking(fast));
  assert(saturated.sent.empty());
  saturated.blocked = false;
  saturated.ParseMessages();
  assert(saturated.sent.size() == 1);
  saturated.sent.clear();
  saturated.blocked = true;
  assert(saturated.SendMsgNonBlocking(fast));
  assert(saturated.sent.empty());
  saturated.blocked = false;
  saturated.ParseMessages();
  assert(saturated.sent.size() == 3);
  assert(saturated.sent[0].first == 0);
  assert(saturated.sent[1].first == 1);
  assert(saturated.sent[2].first == 2);
  for (const auto &frame : saturated.sent) assert(!frame.waited);

  // Existing buffered SendMsg keeps its queued fast-packet wait policy.
  TestBus buffered;
  open(buffered);
  buffered.blocked = true;
  assert(buffered.SendMsg(fast));
  buffered.blocked = false;
  buffered.ParseMessages();
  assert(buffered.sent.size() == 3);
  for (const auto &frame : buffered.sent) assert(frame.waited);
}
