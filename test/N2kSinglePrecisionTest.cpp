#include <catch.hpp>
#include <N2kMsg.h>
#include <N2kMessages.h>

namespace {
bool same_wire_bytes(const tN2kMsg &a, const tN2kMsg &b) {
  if (a.PGN != b.PGN || a.DataLen != b.DataLen || a.Priority != b.Priority) return false;
  for (int i = 0; i < a.DataLen; ++i) {
    if (a.Data[i] != b.Data[i]) return false;
  }
  return true;
}
}

TEST_CASE("single-precision two-byte fields match existing wire encoding") {
  tN2kMsg single;
  tN2kMsg doubled;
  single.Add2ByteSingle(-12.5f, 0.5f);
  doubled.Add2ByteDouble(-12.5, 0.5);
  single.Add2ByteUSingle(12.5f, 0.5f);
  doubled.Add2ByteUDouble(12.5, 0.5);
  REQUIRE(single.DataLen == 4);
  REQUIRE(doubled.DataLen == 4);
  for (int i = 0; i < single.DataLen; ++i) REQUIRE(single.Data[i] == doubled.Data[i]);
  REQUIRE(single.Data[0] == 0xe7);
  REQUIRE(single.Data[1] == 0xff);
  REQUIRE(single.Data[2] == 0x19);
  REQUIRE(single.Data[3] == 0x00);

  int at = 0;
  REQUIRE(single.Get2ByteSingle(0.5f, at) == -12.5f);
  REQUIRE(single.Get2ByteUSingle(0.5f, at) == 12.5f);
  REQUIRE(at == 4);
  REQUIRE(single.Get2ByteSingle(0.5f, at, 42.0f) == 42.0f);
  REQUIRE(at == 4);
}

TEST_CASE("single-precision not-available and overflow values retain sentinels") {
  tN2kMsg msg;
  msg.Add2ByteSingle(N2kFloatNA, 0.5f);
  msg.Add2ByteUSingle(N2kFloatNA, 0.5f);
  REQUIRE(msg.Data[0] == 0xff);
  REQUIRE(msg.Data[1] == 0x7f);
  REQUIRE(msg.Data[2] == 0xff);
  REQUIRE(msg.Data[3] == 0xff);
  int at = 0;
  REQUIRE(msg.Get2ByteSingle(0.5f, at, 37.0f) == 37.0f);
  REQUIRE(msg.Get2ByteUSingle(0.5f, at, 38.0f) == 38.0f);

  tN2kMsg overflow;
  tN2kMsg doubled;
  overflow.Add2ByteSingle(50000.0f, 1.0f);
  doubled.Add2ByteDouble(50000.0, 1.0);
  overflow.Add2ByteUSingle(-1.0f, 1.0f);
  doubled.Add2ByteUDouble(-1.0, 1.0);
  for (int i = 0; i < overflow.DataLen; ++i) REQUIRE(overflow.Data[i] == doubled.Data[i]);
}

TEST_CASE("standard single-precision PGN helpers retain their message layout") {
  tN2kMsg single;
  tN2kMsg doubled;

  SetN2kPGN127250(single, 7, 0.5f, -0.25f, 0.125f, N2khr_true);
  SetN2kPGN127250(doubled, 7, 0.5, -0.25, 0.125, N2khr_true);
  REQUIRE(same_wire_bytes(single, doubled));
  unsigned char sid;
  float heading, deviation, variation;
  tN2kHeadingReference heading_ref;
  REQUIRE(ParseN2kHeading(single, sid, heading, deviation, variation, heading_ref));
  REQUIRE(sid == 7);
  REQUIRE(heading == Approx(0.5f).margin(0.0001f));

  SetN2kPGN127257(single, 8, 0.5f, -0.25f, 0.125f);
  SetN2kPGN127257(doubled, 8, 0.5, -0.25, 0.125);
  REQUIRE(same_wire_bytes(single, doubled));

  SetN2kPGN127258(single, 9, N2kmagvar_Manual, 12, 0.5f);
  SetN2kPGN127258(doubled, 9, N2kmagvar_Manual, 12, 0.5);
  REQUIRE(same_wire_bytes(single, doubled));

  SetN2kPGN128259(single, 10, 2.5f, 1.25f, N2kSWRT_Paddle_wheel);
  SetN2kPGN128259(doubled, 10, 2.5, 1.25, N2kSWRT_Paddle_wheel);
  REQUIRE(same_wire_bytes(single, doubled));

  SetN2kPGN130306(single, 11, 2.5f, 0.5f, N2kWind_Apparent);
  SetN2kPGN130306(doubled, 11, 2.5, 0.5, N2kWind_Apparent);
  REQUIRE(same_wire_bytes(single, doubled));
  float speed, angle;
  tN2kWindReference wind_ref;
  REQUIRE(ParseN2kWindSpeed(single, sid, speed, angle, wind_ref));
  REQUIRE(sid == 11);
  REQUIRE(speed == Approx(2.5f).margin(0.01f));
  REQUIRE(angle == Approx(0.5f).margin(0.0001f));
  REQUIRE(wind_ref == N2kWind_Apparent);
}
