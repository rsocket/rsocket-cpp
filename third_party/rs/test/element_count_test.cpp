// Copyright 2017 Per Grön. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <catch.hpp>

#include <rs/element_count.h>

namespace shk {

TEST_CASE("ElementCount") {
  static constexpr auto kMax = std::numeric_limits<ElementCount::Value>::max();
  static constexpr auto kMin = std::numeric_limits<ElementCount::Value>::min();

  SECTION("construct") {
    SECTION("default") {
      ElementCount count;
      CHECK(count.Get() == 0);
    }

    SECTION("from integer") {
      ElementCount count(1);
      CHECK(count.Get() == 1);
    }

    SECTION("copy") {
      ElementCount count;
      ElementCount copy(count);
    }

    SECTION("move") {
      ElementCount count;
      ElementCount moved(std::move(count));
    }
  }

  SECTION("assignment") {
    SECTION("copy assign") {
      ElementCount a;
      ElementCount b;
      a = b;
    }

    SECTION("move assign") {
      ElementCount a;
      ElementCount b;
      a = std::move(b);
    }

    SECTION("assign value") {
      ElementCount a;
      CHECK((a = 5).Get() == 5);
    }
  }

  SECTION("unbounded") {
    CHECK(ElementCount::Unbounded().Get() == kMax);
    CHECK(ElementCount::Unbounded().IsUnbounded());
    CHECK(ElementCount(kMax).IsUnbounded());
    CHECK(!ElementCount(0).IsUnbounded());
    CHECK(!ElementCount(kMax - 1).IsUnbounded());
    CHECK(!ElementCount(kMin).IsUnbounded());
  }

  SECTION("prefix increment") {
    SECTION("from zero") {
      ElementCount zero;
      CHECK((++zero).Get() == 1);
      CHECK(zero.Get() == 1);
    }

    SECTION("from one") {
      ElementCount one(1);
      CHECK((++one).Get() == 2);
      CHECK(one.Get() == 2);
    }

    SECTION("from unbounded") {
      ElementCount one(kMax);
      CHECK((++one).Get() == kMax);
      CHECK(one.Get() == kMax);
    }

    SECTION("from min value") {
      ElementCount min(kMin);
      CHECK((++min).Get() == kMin + 1);
      CHECK(min.Get() == kMin + 1);
    }
  }

  SECTION("postfix increment") {
    SECTION("from zero") {
      ElementCount zero;
      CHECK((zero++).Get() == 0);
      CHECK(zero.Get() == 1);
    }

    SECTION("from one") {
      ElementCount one(1);
      CHECK((one++).Get() == 1);
      CHECK(one.Get() == 2);
    }

    SECTION("from unbounded") {
      ElementCount one(kMax);
      CHECK((one++).Get() == kMax);
      CHECK(one.Get() == kMax);
    }

    SECTION("from min value") {
      ElementCount min(kMin);
      CHECK((min++).Get() == kMin);
      CHECK(min.Get() == kMin + 1);
    }
  }

  SECTION("prefix decrement") {
    SECTION("from zero") {
      ElementCount zero;
      CHECK((--zero).Get() == -1);
      CHECK(zero.Get() == -1);
    }

    SECTION("from one") {
      ElementCount one(1);
      CHECK((--one).Get() == 0);
      CHECK(one.Get() == 0);
    }

    SECTION("from unbounded") {
      ElementCount one(kMax);
      CHECK((--one).Get() == kMax);
      CHECK(one.Get() == kMax);
    }

    SECTION("from min value") {
      ElementCount min(kMin);
      CHECK_THROWS_AS(--min, std::range_error);
    }
  }

  SECTION("addition assignment with value") {
    SECTION("from zero") {
      ElementCount zero;
      CHECK((zero += 2).Get() == 2);
      CHECK(zero.Get() == 2);
    }

    SECTION("negative from zero") {
      ElementCount zero;
      CHECK((zero += -2).Get() == -2);
      CHECK(zero.Get() == -2);
    }

    SECTION("from one") {
      ElementCount one(1);
      CHECK((one += 2).Get() == 3);
      CHECK(one.Get() == 3);
    }

    SECTION("negative from one") {
      ElementCount one(1);
      CHECK((one += -2).Get() == -1);
      CHECK(one.Get() == -1);
    }

    SECTION("from unbounded") {
      ElementCount one(kMax);
      CHECK((one += 2).Get() == kMax);
      CHECK(one.Get() == kMax);
    }

    SECTION("negative from unbounded") {
      ElementCount one(kMax);
      CHECK((one += -2).Get() == kMax);
      CHECK(one.Get() == kMax);
    }

    SECTION("take unbounded from unbounded") {
      ElementCount one(kMax);
      CHECK((one += kMin).Get() == kMax);
      CHECK(one.Get() == kMax);
    }

    SECTION("from min value") {
      ElementCount min(kMin);
      CHECK((min += 2).Get() == kMin + 2);
      CHECK(min.Get() == kMin + 2);
    }

    SECTION("negative from min value") {
      ElementCount min(kMin);
      CHECK_THROWS_AS(min += -1, std::range_error);
    }
  }

  SECTION("addition assignment with ElementCount") {
    SECTION("from zero") {
      ElementCount zero;
      CHECK((zero += ElementCount(2)).Get() == 2);
      CHECK(zero.Get() == 2);
    }

    // This shares logic with addition assignment with value so no need to test
    // all corner cases here.
  }

  SECTION("subtraction assignment with value") {
    SECTION("from zero") {
      ElementCount zero;
      CHECK((zero -= 2).Get() == -2);
      CHECK(zero.Get() == -2);
    }

    SECTION("negative from zero") {
      ElementCount zero;
      CHECK((zero -= -2).Get() == 2);
      CHECK(zero.Get() == 2);
    }

    SECTION("from one") {
      ElementCount one(1);
      CHECK((one -= 2).Get() == -1);
      CHECK(one.Get() == -1);
    }

    SECTION("negative from one") {
      ElementCount one(1);
      CHECK((one -= -2).Get() == 3);
      CHECK(one.Get() == 3);
    }

    SECTION("from unbounded") {
      ElementCount one(kMax);
      CHECK((one -= 2).Get() == kMax);
      CHECK(one.Get() == kMax);
    }

    SECTION("take unbounded from unbounded") {
      ElementCount one(kMax);
      CHECK((one -= kMax).Get() == kMax);
      CHECK(one.Get() == kMax);
    }

    SECTION("negative from unbounded") {
      ElementCount one(kMax);
      CHECK((one -= -2).Get() == kMax);
      CHECK(one.Get() == kMax);
    }

    SECTION("from min value") {
      ElementCount min(kMin);
      CHECK_THROWS_AS(min -= 1, std::range_error);
    }

    SECTION("negative from min value") {
      ElementCount min(kMin);
      CHECK((min -= -2).Get() == kMin + 2);
      CHECK(min.Get() == kMin + 2);
    }
  }

  SECTION("subtraction assignment with ElementCount") {
    SECTION("from zero") {
      ElementCount zero;
      CHECK((zero -= ElementCount(2)).Get() == -2);
      CHECK(zero.Get() == -2);
    }

    // This shares logic with subtraction assignment with value so no need to
    // test all corner cases here.
  }

  SECTION("addition") {
    // This shares logic with addition assignment with value so no need to
    // test all corner cases here.

    CHECK((ElementCount(1) + ElementCount(2)).Get() == 3);
    CHECK((ElementCount(1) + 2).Get() == 3);
    CHECK((1 + ElementCount(2)).Get() == 3);
  }

  SECTION("subtraction") {
    // This shares logic with subtraction assignment with value so no need to
    // test all corner cases here.

    CHECK((ElementCount(3) - ElementCount(1)).Get() == 2);
    CHECK((ElementCount(3) - 1).Get() == 2);
    CHECK((3 - ElementCount(1)).Get() == 2);
  }

  SECTION("equality") {
    SECTION("ElementCount vs ElementCount") {
      CHECK(ElementCount(0) == ElementCount(0));
      CHECK(ElementCount::Unbounded() == ElementCount::Unbounded());
      CHECK(!(ElementCount(0) == ElementCount(1)));

      CHECK(!(ElementCount(0) != ElementCount(0)));
      CHECK(!(ElementCount::Unbounded() != ElementCount::Unbounded()));
      CHECK(ElementCount(0) != ElementCount(1));
    }

    SECTION("ElementCount vs Value") {
      CHECK(ElementCount(0) == 0);
      CHECK(ElementCount::Unbounded() == kMax);
      CHECK(!(ElementCount(0) == 1));

      CHECK(!(ElementCount(0) != 0));
      CHECK(
          !(ElementCount::Unbounded() != kMax));
      CHECK(ElementCount(0) != 1);
    }

    SECTION("Value vs ElementCount") {
      CHECK(0 == ElementCount(0));
      CHECK(kMax == ElementCount::Unbounded());
      CHECK(!(0 == ElementCount(1)));

      CHECK(!(0 != ElementCount(0)));
      CHECK(
          !(kMax != ElementCount::Unbounded()));
      CHECK(0 != ElementCount(1));
    }
  }

  SECTION("compare") {
    SECTION("ElementCount vs ElementCount") {
      SECTION("less than") {
        CHECK(ElementCount(0) < ElementCount(1));
        CHECK(!(ElementCount(0) < ElementCount(0)));
        CHECK(!(ElementCount(1) < ElementCount(0)));
        CHECK(ElementCount(0) < ElementCount::Unbounded());
        CHECK(!(ElementCount::Unbounded() < ElementCount::Unbounded()));
      }

      SECTION("less than or equal") {
        CHECK(ElementCount(0) <= ElementCount(1));
        CHECK(ElementCount(0) <= ElementCount(0));
        CHECK(!(ElementCount(1) <= ElementCount(0)));
        CHECK(ElementCount(0) <= ElementCount::Unbounded());
        CHECK(ElementCount::Unbounded() <= ElementCount::Unbounded());
      }

      SECTION("greater than") {
        CHECK(ElementCount(1) > ElementCount(0));
        CHECK(!(ElementCount(0) > ElementCount(0)));
        CHECK(!(ElementCount(0) > ElementCount(1)));
        CHECK(ElementCount::Unbounded() > ElementCount(0));
        CHECK(!(ElementCount::Unbounded() > ElementCount::Unbounded()));
      }

      SECTION("greater than or equal") {
        CHECK(ElementCount(1) >= ElementCount(0));
        CHECK(ElementCount(0) >= ElementCount(0));
        CHECK(!(ElementCount(0) >= ElementCount(1)));
        CHECK(ElementCount::Unbounded() >= ElementCount(0));
        CHECK(ElementCount::Unbounded() >= ElementCount::Unbounded());
      }
    }

    SECTION("ElementCount vs Value") {
      SECTION("less than") {
        CHECK(ElementCount(0) < 1);
        CHECK(!(ElementCount(0) < 0));
        CHECK(!(ElementCount(1) < 0));
        CHECK(ElementCount(0) < kMax);
        CHECK(!(ElementCount::Unbounded() < kMax));
      }

      SECTION("less than or equal") {
        CHECK(ElementCount(0) <= 1);
        CHECK(ElementCount(0) <= 0);
        CHECK(!(ElementCount(1) <= 0));
        CHECK(ElementCount(0) <= kMax);
        CHECK(ElementCount::Unbounded() <= kMax);
      }

      SECTION("greater than") {
        CHECK(ElementCount(1) > 0);
        CHECK(!(ElementCount(0) > 0));
        CHECK(!(ElementCount(0) > 1));
        CHECK(ElementCount::Unbounded() > 0);
        CHECK(!(ElementCount::Unbounded() > kMax));
      }

      SECTION("greater than or equal") {
        CHECK(ElementCount(1) >= 0);
        CHECK(ElementCount(0) >= 0);
        CHECK(!(ElementCount(0) >= 1));
        CHECK(ElementCount::Unbounded() >= 0);
        CHECK(ElementCount::Unbounded() >= kMax);
      }
    }

    SECTION("Value vs ElementCount") {
      SECTION("less than") {
        CHECK(0 < ElementCount(1));
        CHECK(!(0 < ElementCount(0)));
        CHECK(!(1 < ElementCount(0)));
        CHECK(0 < ElementCount::Unbounded());
        CHECK(!(kMax < ElementCount::Unbounded()));
      }

      SECTION("less than or equal") {
        CHECK(0 <= ElementCount(1));
        CHECK(0 <= ElementCount(0));
        CHECK(!(1 <= ElementCount(0)));
        CHECK(0 <= ElementCount::Unbounded());
        CHECK(kMax <= ElementCount::Unbounded());
      }

      SECTION("greater than") {
        CHECK(1 > ElementCount(0));
        CHECK(!(0 > ElementCount(0)));
        CHECK(!(0 > ElementCount(1)));
        CHECK(kMax > ElementCount(0));
        CHECK(!(kMax > ElementCount::Unbounded()));
      }

      SECTION("greater than or equal") {
        CHECK(1 >= ElementCount(0));
        CHECK(0 >= ElementCount(0));
        CHECK(!(0 >= ElementCount(1)));
        CHECK(kMax >= ElementCount(0));
        CHECK(kMax >= ElementCount::Unbounded());
      }
    }
  }
}

}  // namespace shk
