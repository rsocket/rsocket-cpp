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

#include <rs/concat.h>

namespace shk {

auto Prepend() {
  return [](auto &&stream) {
    return std::move(stream);
  };
}

template <typename Publisher>
auto Prepend(Publisher &&prepended_publisher) {
  return [prepended_publisher = std::forward<Publisher>(prepended_publisher)](
      auto &&stream) {
    return Concat(
        prepended_publisher,
        std::forward<decltype(stream)>(stream));
  };
}

template <typename Publisher0, typename Publisher1, typename ...Publishers>
auto Prepend(
    Publisher0 &&publisher_0,
    Publisher1 &&publisher_1,
    Publishers &&...publishers) {
  return Prepend(Concat(
      std::forward<Publisher0>(publisher_0),
      std::forward<Publisher1>(publisher_1),
      std::forward<Publishers>(publishers)...));
}

}  // namespace shk
