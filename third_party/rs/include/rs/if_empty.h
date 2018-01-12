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

#include <memory>
#include <type_traits>

#include <rs/concat.h>
#include <rs/empty.h>
#include <rs/map.h>
#include <rs/publisher.h>
#include <rs/subscription.h>

namespace shk {

/**
 * Takes a stream of values and makes a stream of values that has all the values
 * in that stream. If that stream turns out to be empty, another stream is
 * concatenated to it.
 */
template <typename Publisher>
auto IfEmpty(Publisher &&publisher) {
  static_assert(
      IsPublisher<typename std::decay<Publisher>::type>,
      "IfEmpty must be given a Publisher");

  // Return an operator (it takes a Publisher and returns a Publisher)
  return [publisher = std::forward<Publisher>(publisher)](auto source) {
    auto empty = std::make_shared<bool>(true);
    auto mapper = Map([empty](auto &&value) {
      *empty = false;
      return std::forward<decltype(value)>(value);
    });
    auto publisher_if_empty = MakePublisher([
        publisher, empty](auto &&subscriber) {
      if (*empty) {
        return AnySubscription(publisher.Subscribe(
            std::forward<decltype(subscriber)>(subscriber)));
      } else {
        return AnySubscription(Empty().Subscribe(
            std::forward<decltype(subscriber)>(subscriber)));
      }
    });

    return Concat(mapper(source), publisher_if_empty);
  };
}

}  // namespace shk
