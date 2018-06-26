// Copyright (c) Facebook, Inc. and its affiliates.
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

#include <benchmark/benchmark.h>
#include <iostream>
#include <memory>

/*
 * Seeking to understand cost of different method signatures
 * when passing an object through levels of functions.
 *
 * function_nested_copy                       93 ns         78 ns    9909119
 * function_nested_move                       71 ns         66 ns   11116405
 * function_nested_ref                        84 ns         74 ns   11032483
 * function_nested_unique_ptr                364 ns        344 ns    1934567
 */

struct Tuple {
  Tuple(int xx, int yy) : x(xx), y(yy) {}
  const int x;
  const int y;

  void doSomething() {}
};

void functionByCopyAgain(Tuple a) {
  a.doSomething();
}

void functionByCopy(Tuple a) {
  functionByCopyAgain(a);
}

static void function_nested_copy(benchmark::State& state) {
  while (state.KeepRunning()) {
    Tuple a(1, 2);
    functionByCopy(a);
  }
}
BENCHMARK(function_nested_copy);

void functionByMoveAgain(Tuple a) {
  a.doSomething();
}

void functionByMove(Tuple a) {
  functionByMoveAgain(std::move(a));
}

static void function_nested_move(benchmark::State& state) {
  while (state.KeepRunning()) {
    Tuple a(1, 2);
    functionByCopy(std::move(a));
  }
}
BENCHMARK(function_nested_move);

void functionByRefAgain(Tuple& a) {
  a.doSomething();
}

void functionByRef(Tuple& a) {
  functionByRefAgain(a);
}

static void function_nested_ref(benchmark::State& state) {
  while (state.KeepRunning()) {
    Tuple a(1, 2);
    functionByCopy(a);
  }
}
BENCHMARK(function_nested_ref);

void functionByUniquePtrAgain(std::unique_ptr<Tuple> a) {
  a->doSomething();
}

void functionByUniquePtr(std::unique_ptr<Tuple> a) {
  functionByUniquePtrAgain(std::move(a));
}

static void function_nested_unique_ptr(benchmark::State& state) {
  while (state.KeepRunning()) {
    functionByUniquePtr(std::make_unique<Tuple>(1, 2));
  }
}
BENCHMARK(function_nested_unique_ptr);

BENCHMARK_MAIN()
