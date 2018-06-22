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

#include "yarpl/test_utils/Mocks.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;
using namespace yarpl::flowable;
using namespace yarpl::mocks;

TEST(MocksTest, SelfManagedMocks) {
  // Best run with ASAN, to detect potential leaks, use-after-free or
  // double-free bugs.
  int value = 42;

  MockFlowable<int> flowable;
  auto subscription = std::make_shared<yarpl::mocks::MockSubscription>();
  auto subscriber = std::make_shared<yarpl::mocks::MockSubscriber<int>>(0);
  {
    InSequence dummy;
    EXPECT_CALL(flowable, subscribe_(_))
        .WillOnce(Invoke([&](std::shared_ptr<Subscriber<int>> consumer) {
          consumer->onSubscribe(subscription);
        }));
    EXPECT_CALL(*subscriber, onSubscribe_(_));
    EXPECT_CALL(*subscription, request_(1));
    EXPECT_CALL(*subscription, cancel_()).WillOnce(Invoke([&]() {
      // We must have received Subscription::request(1), hence we can
      // deliver one element, despite Subscription::cancel() has been
      // called.
      subscriber->onNext(value);
      // This Publisher never spontaneously terminates the subscription,
      // hence we can respond with onComplete unconditionally.
      subscriber->onComplete();
      subscriber = nullptr;
    }));
    EXPECT_CALL(*subscriber, onNext_(value));
    EXPECT_CALL(*subscriber, onComplete_());
  }
  flowable.subscribe(subscriber);
  subscription->request(1);
  subscription->cancel();
  subscription = nullptr;
}
