#include <catch.hpp>
#include <N2kMsg.h>

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
