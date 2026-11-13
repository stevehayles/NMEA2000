#include "NMEA2000.h"
#include "N2kTimer.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace {
uint64_t now_ms;

class MemoryStream : public N2kStream {
public:
  std::vector<uint8_t> bytes;

  int read() override { return -1; }
  int peek() override { return -1; }
  size_t write(const uint8_t *data, size_t size) override {
    bytes.insert(bytes.end(), data, data + size);
    return size;
  }
};

class TestBus : public tNMEA2000 {
public:
  void forward(const tN2kMsg &msg) { ForwardMessage(msg); }
  void forward(const tN2kCANMsg &msg) { ForwardMessage(msg); }

protected:
  bool CANOpen() override { return true; }
  bool CANSendFrame(unsigned long, unsigned char, const unsigned char *, bool) override {
    return true;
  }
  bool CANGetFrame(unsigned long &, unsigned char &, unsigned char *) override {
    return false;
  }
};

struct Capture {
  unsigned calls = 0;
  const tN2kMsg *message = nullptr;
};

void capture(const tN2kMsg &msg, void *context) {
  Capture &state = *static_cast<Capture *>(context);
  ++state.calls;
  state.message = &msg;
}

void open(TestBus &bus) {
  bus.EnableForward(false);
  bus.SetMode(tNMEA2000::N2km_ListenAndNode, 0x1e);
  for (unsigned i = 0; i < 15 && !bus.IsOpen(); ++i) {
    bus.ParseMessages();
    now_ms += 30;
  }
  assert(bus.IsOpen());
  bus.EnableForward();
}
} // namespace

#if defined(N2K_TEST_TIMER_32)
extern "C" uint32_t millis() { return static_cast<uint32_t>(now_ms); }
#elif defined(N2K_TEST_TIMER_ESP)
uint64_t tN2kSyncScheduler::SyncOffset = 0;
#else
uint64_t tN2kSyncScheduler::SyncOffset = 0;
uint64_t N2kMillis64() { return now_ms; }
uint32_t N2kMillis() { return static_cast<uint32_t>(now_ms); }
#endif
extern "C" int64_t esp_timer_get_time() { return static_cast<int64_t>(now_ms * 1000); }
extern "C" bool rtc_try_get_timeofday(double *) { return false; }

int main() {
  TestBus bus;
  open(bus);
  tN2kMsg msg;
  msg.SetPGN(130306);
  msg.Source = 55;
  msg.AddByte(0x12);

  MemoryStream stream;
  bus.SetForwardStream(&stream);
  bus.forward(msg);
  assert(!stream.bytes.empty());
  const size_t forwarded_size = stream.bytes.size();

  Capture state;
  bus.SetForwardCallback(capture, &state);
  bus.forward(msg);
  assert(state.calls == 1 && state.message == &msg);
  assert(stream.bytes.size() == forwarded_size);

  bus.EnableForward(false);
  bus.forward(msg);
  assert(state.calls == 1 && stream.bytes.size() == forwarded_size);

  bus.EnableForward();
  bus.SetForwardCallback(nullptr);
  bus.forward(msg);
  assert(state.calls == 1 && stream.bytes.size() == forwarded_size * 2);
  assert(std::equal(stream.bytes.begin(), stream.bytes.begin() + forwarded_size,
                    stream.bytes.begin() + forwarded_size));

  bus.SetForwardStream(nullptr);
  bus.SetForwardCallback(capture, &state);
  bus.forward(msg);
  assert(state.calls == 2 && state.message == &msg);

  tN2kCANMsg received;
  received.N2kMsg = msg;
  bus.SetForwardOnlyKnownMessages();
  bus.forward(received);
  assert(state.calls == 2);
  received.KnownMessage = true;
  bus.forward(received);
  assert(state.calls == 3 && state.message == &received.N2kMsg);
  return 0;
}
