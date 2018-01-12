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
#include <rs/just.h>
#include <rs/start.h>

namespace shk {

template <typename ...Values>
auto StartWith(Values &&...values) {
  auto prefix_stream = Just(std::forward<Values>(values)...);
  return [prefix_stream = std::move(prefix_stream)](auto &&stream) {
    return Concat(prefix_stream, std::forward<decltype(stream)>(stream));
  };
}

template <typename ...MakeValues>
auto StartWithGet(MakeValues &&...make_values) {
  auto prefix_stream = Start(std::forward<MakeValues>(make_values)...);
  return [prefix_stream = std::move(prefix_stream)](auto &&stream) {
    return Concat(prefix_stream, std::forward<decltype(stream)>(stream));
  };
}

}  // namespace shk
