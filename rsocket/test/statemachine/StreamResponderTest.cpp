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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <yarpl/test_utils/Mocks.h>

#include "rsocket/statemachine/StreamResponder.h"
#include "rsocket/test/test_utils/MockStreamsWriter.h"

using namespace rsocket;
using namespace testing;
using namespace yarpl::mocks;

TEST(StreamResponder, OnComplete) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto responder = std::make_shared<StreamResponder>(writer, 1u, 0);

  EXPECT_CALL(*writer, writePayload_(_)).Times(3);
  EXPECT_CALL(*writer, onStreamClosed(1u));

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, request_(_));

  responder->onSubscribe(subscription);
  ASSERT_FALSE(responder->publisherClosed());

  subscription->request(2);

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->onComplete();
  ASSERT_TRUE(responder->publisherClosed());
}

TEST(StreamResponder, OnError) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto responder = std::make_shared<StreamResponder>(writer, 1u, 0);

  EXPECT_CALL(*writer, writePayload_(_)).Times(2);
  EXPECT_CALL(*writer, writeError_(_));
  EXPECT_CALL(*writer, onStreamClosed(1u));

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, request_(_));

  responder->onSubscribe(subscription);
  ASSERT_FALSE(responder->publisherClosed());

  subscription->request(2);

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->onError(std::runtime_error{"Test"});
  ASSERT_TRUE(responder->publisherClosed());
}

TEST(StreamResponder, HandleError) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto responder = std::make_shared<StreamResponder>(writer, 1u, 0);

  EXPECT_CALL(*writer, writePayload_(_)).Times(2);
  EXPECT_CALL(*writer, onStreamClosed(1u));

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, request_(_));
  EXPECT_CALL(*subscription, cancel_());

  responder->onSubscribe(subscription);
  ASSERT_FALSE(responder->publisherClosed());

  subscription->request(2);

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->handleError(std::runtime_error("Test"));
  ASSERT_TRUE(responder->publisherClosed());
}

TEST(StreamResponder, HandleCancel) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto responder = std::make_shared<StreamResponder>(writer, 1u, 0);

  EXPECT_CALL(*writer, writePayload_(_)).Times(2);
  EXPECT_CALL(*writer, onStreamClosed(1u));

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, request_(_));
  EXPECT_CALL(*subscription, cancel_());

  responder->onSubscribe(subscription);
  ASSERT_FALSE(responder->publisherClosed());

  subscription->request(2);

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->handleCancel();
  ASSERT_TRUE(responder->publisherClosed());
}

TEST(StreamResponder, EndStream) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto responder = std::make_shared<StreamResponder>(writer, 1u, 0);

  EXPECT_CALL(*writer, writePayload_(_)).Times(2);
  EXPECT_CALL(*writer, writeError_(_));
  EXPECT_CALL(*writer, onStreamClosed(1u));

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, request_(_));
  EXPECT_CALL(*subscription, cancel_());

  responder->onSubscribe(subscription);
  ASSERT_FALSE(responder->publisherClosed());

  subscription->request(2);

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->onNext(Payload{});
  ASSERT_FALSE(responder->publisherClosed());

  responder->endStream(StreamCompletionSignal::SOCKET_CLOSED);
  ASSERT_TRUE(responder->publisherClosed());
}
