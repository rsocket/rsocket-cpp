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

#pragma once

#include <utility>
#include <type_traits>

#include <rs/concat.h>
#include <rs/publisher.h>

namespace shk {

auto Append() {
  return [](auto &&stream) {
    static_assert(
        IsPublisher<typename std::decay<decltype(stream)>::type>,
        "Argument must be Publisher");
    return std::move(stream);
  };
}

template <typename Publisher>
auto Append(Publisher &&appended_publisher) {
  return [appended_publisher = std::forward<Publisher>(appended_publisher)](
      auto &&stream) {
    return Concat(
        std::forward<decltype(stream)>(stream),
        appended_publisher);
  };
}

template <typename Publisher0, typename Publisher1, typename ...Publishers>
auto Append(
    Publisher0 &&publisher_0,
    Publisher1 &&publisher_1,
    Publishers &&...publishers) {
  return Append(Concat(
      std::forward<Publisher0>(publisher_0),
      std::forward<Publisher1>(publisher_1),
      std::forward<Publishers>(publishers)...));
}

}  // namespace shk
