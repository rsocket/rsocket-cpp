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

#include <folly/Portability.h>
#include <folly/io/async/EventBase.h>
#include <gtest/gtest.h>
#include <vector>

#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"
#include "yarpl/flowable/ThriftStreamShim.h"

using namespace yarpl::flowable;

template <typename T>
std::vector<T> run(
    std::shared_ptr<Flowable<T>> flowable,
    int64_t requestCount = 100) {
  auto subscriber = std::make_shared<TestSubscriber<T>>(requestCount);
  flowable->subscribe(subscriber);
  subscriber->awaitTerminalEvent(std::chrono::seconds(1));
  return std::move(subscriber->values());
}
template <typename T>
std::vector<T> run(apache::thrift::ServerStream<T>&& stream) {
  std::vector<T> values;
  std::move(stream).toClientStreamUnsafeDoNotUse().subscribeInline([&](auto&& val) {
    if (val.hasValue()) {
      values.push_back(std::move(*val));
    }
  });
  return values;
}

apache::thrift::ClientBufferedStream<int> makeRange(int start, int count) {
  auto streamAndPublisher =
      apache::thrift::ServerStream<int>::createPublisher();
  for (int i = 0; i < count; ++i) {
    streamAndPublisher.second.next(i + start);
  }
  std::move(streamAndPublisher.second).complete();
  return std::move(streamAndPublisher.first).toClientStreamUnsafeDoNotUse();
}

TEST(ThriftStreamShimTest, ClientStream) {
  auto flowable = ThriftStreamShim::fromClientStream(
      makeRange(1, 5), folly::getEventBase());
  EXPECT_EQ(run(flowable), std::vector<int>({1, 2, 3, 4, 5}));
}

TEST(ThriftStreamShimTest, ServerStream) {
  auto stream = ThriftStreamShim::toServerStream(Flowable<>::range(1, 5));
  EXPECT_EQ(run(std::move(stream)), std::vector<long>({1, 2, 3, 4, 5}));

  stream = ThriftStreamShim::toServerStream(Flowable<long>::never());
  auto sub = std::move(stream).toClientStreamUnsafeDoNotUse().subscribeExTry(
      folly::getEventBase(), [](auto) {});
  sub.cancel();
  std::move(sub).join();

  ThriftStreamShim::toServerStream(Flowable<>::just(std::make_unique<int>(42)));
}
