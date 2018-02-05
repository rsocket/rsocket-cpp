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

#include <type_traits>

namespace shk {
namespace detail {

template <typename ...Operators>
class PipeOperator;

template <>
class PipeOperator<> {
 public:
  template <typename T>
  auto operator()(T &&t) const {
    return std::forward<T>(t);
  }
};

template <typename Operator, typename ...Operators>
class PipeOperator<Operator, Operators...> {
 public:
  template <typename OperatorT, typename ...OperatorTs>
  PipeOperator(OperatorT &&op, OperatorTs &&...ops)
      : op_(std::forward<OperatorT>(op)),
        inner_ops_(std::forward<OperatorTs>(ops)...) {}

  template <typename T>
  auto operator()(T &&t) {
    return inner_ops_(op_(std::forward<T>(t)));
  }

  template <typename T>
  auto operator()(T &&t) const {
    return inner_ops_(op_(std::forward<T>(t)));
  }

 private:
  Operator op_;
  PipeOperator<Operators...> inner_ops_;
};

}  // namespace detail

/**
 * BuildPipe is a helper function that makes it easy to pipe operators through
 * each other. It takes a bunch of operators and returns one that strings them
 * through each other, one by one.
 *
 * BuildPipe(a, b, c) is roughly equal to [](auto x) { return c(b(a(x))); }
 *
 * An example of usage, constructing an operator that takes a stream makes a
 * stream of the sum of squares in the inner stream:
 *
 *     BuildPipe(
 *         Map([](int x) { return x * x; }),
 *         Sum())
 */
template <typename ...Operators>
auto BuildPipe(Operators &&...operators) {
  return detail::PipeOperator<typename std::decay<Operators>::type...>(
      std::forward<Operators>(operators)...);
}

/**
 * Pipe is like BuildPipe but instead of returning a function that takes a value
 * it directly takes the value to pipe through the given operators.
 *
 * Pipe(a, b, c) is roughly equal to c(b(a))
 *
 * An example of usage, constructing a stream that has all even numbers from 0
 * to 99:
 *
 *     Pipe(
 *         Range(0, 100),
 *         Filter([](int v) { return (v % 2) == 0; }))
 */
template <typename StartValue, typename ...Operators>
auto Pipe(StartValue &&start_value, Operators &&...operators) {
  return BuildPipe(std::forward<Operators>(operators)...)(
      std::forward<StartValue>(start_value));
}

}  // namespace shk
