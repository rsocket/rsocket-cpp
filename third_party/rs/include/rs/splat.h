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

#include <tuple>

namespace shk {
namespace detail {

template <size_t Idx>
class InvokeSplatCallback {
 public:
  template <
      typename Callback,
      typename Tuple,
      typename ...Parameters>
  static auto Invoke(
      Callback &&callback,
      Tuple &&tuple,
      Parameters &&...parameters) {
    static constexpr size_t tuple_size = std::tuple_size<
        typename std::decay<decltype(tuple)>::type>::value;

    return InvokeSplatCallback<Idx - 1>::Invoke(
        std::forward<Callback>(callback),
        std::forward<Tuple>(tuple),
        std::forward<Parameters>(parameters)...,
        std::get<tuple_size - Idx>(std::forward<Tuple>(tuple)));
  }
};

template <>
class InvokeSplatCallback<0> {
 public:
  template <
      typename Callback,
      typename Tuple,
      typename ...Parameters>
  static auto Invoke(
      Callback &&callback,
      Tuple &&tuple,
      Parameters &&...parameters) {
    return callback(std::forward<Parameters>(parameters)...);
  }
};

}  // namespace detail

/**
 * Splat is a helper function that can make it easier to access the indivicual
 * elements of a tuple or a pair. What it does is similar to std::tie, but it is
 * meant to be used in a different context. In cases where you would write:
 *
 *     [](std::tuple<int, std::string> t) {
 *       auto &num = std::get<0>(t);
 *       auto &str = std::get<1>(t);
 *
 *       ...
 *     }
 *
 * You could use Splat and instead write:
 *
 *     Splat([](int num, std::string str) {
 *       ...
 *     })
 *
 * This is particularly useful when dealing with streams that have tuples, for
 * example because of Zip:
 *
 *     Pipe(
 *         Zip(Just(1, 2), Just("a", "b"))
 *         Map(Splat([](int num, std::string str) {
 *           return str + " " + std::to_string(num);
 *         })))
 *
 * Splat works with all types for which std::tuple_size and std::get are
 * defined: In addition to tuples it also works with pair<>s and array<>s.
 */
template <typename Callback>
auto Splat(Callback &&callback) {
  return [callback = std::forward<Callback>(callback)](auto &&tuple) mutable {
    static constexpr size_t tuple_size = std::tuple_size<
        typename std::decay<decltype(tuple)>::type>::value;
    return detail::InvokeSplatCallback<tuple_size>::Invoke(
        callback,
        std::forward<decltype(tuple)>(tuple));
  };
}

}  // namespace shk
