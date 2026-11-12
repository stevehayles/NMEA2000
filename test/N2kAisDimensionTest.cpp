#include "N2kMessages.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>

// This wire-vector test has no scheduler; the message constructor still stamps time.
uint32_t N2kMillis() { return 0; }

namespace {

void expect_dimension_bytes(const tN2kMsg &message, int offset, uint16_t raw) {
  assert(message.DataLen >= offset + 2);
  if (message.Data[offset] != (raw & 0xff) || message.Data[offset + 1] != (raw >> 8)) {
    std::fprintf(stderr, "PGN %lu offset %d: %02x %02x, expected %04x\n", message.PGN,
                 offset, message.Data[offset], message.Data[offset + 1], raw);
  }
  assert(message.Data[offset] == (raw & 0xff));
  assert(message.Data[offset + 1] == (raw >> 8));
}

void expect_close(double actual, double expected) {
  assert(std::fabs(actual - expected) < 0.0001);
}

void set_class_a(tN2kMsg &message, double length) {
  SetN2kPGN129794(message, 5, N2kaisr_Initial, 123456789, 1234567, "CALL123", "TEST VESSEL",
                  37, length, 12.3, 4.5, 6.7, 20000, 3600.0, 2.1, "TEST",
                  static_cast<tN2kAISVersion>(0), N2kGNSSt_GPS,
                  static_cast<tN2kAISDTE>(0));
}

} // namespace

int main() {
  tN2kAISAtoNReportData aid;
  aid.MessageID = 21;
  aid.UserID = 123456789;
  aid.Longitude = -1.0;
  aid.Latitude = 50.0;
  aid.Seconds = 12;
  aid.Length = 100.1;
  aid.Beam = 12.3;
  aid.PositionReferenceStarboard = 4.5;
  aid.PositionReferenceTrueNorth = 6.7;
  aid.SetAtoNName("TEST AID");

  tN2kMsg aid_message;
  SetN2kPGN129041(aid_message, aid);
  assert(aid_message.PGN == 129041);
  expect_dimension_bytes(aid_message, 14, 1001);
  expect_dimension_bytes(aid_message, 16, 123);
  expect_dimension_bytes(aid_message, 18, 45);
  expect_dimension_bytes(aid_message, 20, 67);

  tN2kAISAtoNReportData parsed_aid;
  assert(ParseN2kPGN129041(aid_message, parsed_aid));
  expect_close(parsed_aid.Length, aid.Length);
  expect_close(parsed_aid.Beam, aid.Beam);
  expect_close(parsed_aid.PositionReferenceStarboard, aid.PositionReferenceStarboard);
  expect_close(parsed_aid.PositionReferenceTrueNorth, aid.PositionReferenceTrueNorth);

  tN2kMsg vessel_message;
  set_class_a(vessel_message, 100.1);
  assert(vessel_message.PGN == 129794);
  expect_dimension_bytes(vessel_message, 37, 1001);
  expect_dimension_bytes(vessel_message, 39, 123);
  expect_dimension_bytes(vessel_message, 41, 45);
  expect_dimension_bytes(vessel_message, 43, 67);

  // AtoN already uses unsigned dimensions in Bluefin. Upstream also corrects
  // class A static dimensions to unsigned; valid values keep the same bytes.
#ifdef N2K_AIS_UNSIGNED_DIMENSIONS
  constexpr uint16_t unavailable = 0xffff;
#else
  constexpr uint16_t unavailable = 0x7fff;
#endif

  aid.Length = N2kDoubleNA;
  SetN2kPGN129041(aid_message, aid);
  expect_dimension_bytes(aid_message, 14, 0xffff);

  set_class_a(vessel_message, N2kDoubleNA);
  expect_dimension_bytes(vessel_message, 37, unavailable);
}
