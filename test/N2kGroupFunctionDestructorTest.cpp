#include "catch.hpp"
#include "N2kGroupFunction.h"

namespace {
struct OwnedHandler : tN2kGroupFunctionHandler {
  bool &destroyed;

  explicit OwnedHandler(bool &flag) : tN2kGroupFunctionHandler(nullptr, 0), destroyed(flag) {}
  ~OwnedHandler() override { destroyed = true; }
};
}

TEST_CASE("derived group function handler is destroyed through its base pointer") {
  bool destroyed = false;
  tN2kGroupFunctionHandler *handler = new OwnedHandler(destroyed);
  delete handler;
  REQUIRE(destroyed);
}
