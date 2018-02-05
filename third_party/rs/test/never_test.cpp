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

#include <rs/never.h>
#include <rs/subscriber.h>

namespace shk {

TEST_CASE("Never") {
  SECTION("construct") {
    auto never = Never();
    static_assert(
        IsPublisher<decltype(never)>,
        "Never stream should be a publisher");
  }

  SECTION("subscribe") {
    auto never = Never();

    auto subscription = never.Subscribe(MakeSubscriber(
        [](int next) { CHECK(!"should not happen"); },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [] { CHECK(!"should not happen"); }));

    subscription.Request(ElementCount(0));
    subscription.Request(ElementCount(1));
    subscription.Request(ElementCount::Unbounded());
  }
}

}  // namespace shk
