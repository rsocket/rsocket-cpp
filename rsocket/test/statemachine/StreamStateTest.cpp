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
#include "rsocket/internal/Common.h"
#include "rsocket/statemachine/ChannelRequester.h"
#include "rsocket/statemachine/ChannelResponder.h"
#include "rsocket/statemachine/StreamStateMachineBase.h"
#include "rsocket/test/test_utils/MockStreamsWriter.h"

using namespace rsocket;
using namespace testing;
using namespace yarpl::mocks;

class TestStreamStateMachineBase : public StreamStateMachineBase {
 public:
  using StreamStateMachineBase::StreamStateMachineBase;
  void handlePayload(Payload&&, bool, bool, bool) override {
    // ignore...
  }
};

// @see github.com/rsocket/rsocket/blob/master/Protocol.md#request-channel
TEST(StreamState, NewStateMachineBase) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  EXPECT_CALL(*writer, onStreamClosed(_));

  TestStreamStateMachineBase ssm(writer, 1u);
  ssm.getConsumerAllowance();
  ssm.handleCancel();
  ssm.handleError(std::runtime_error("test"));
  ssm.handlePayload(Payload{}, false, true, false);
  ssm.handleRequestN(1);
}

TEST(StreamState, ChannelRequesterOnError) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto requester = std::make_shared<ChannelRequester>(writer, 1u);

  EXPECT_CALL(*writer, writeNewStream_(1u, _, _, _));
  EXPECT_CALL(*writer, writeError_(_));
  EXPECT_CALL(*writer, onStreamClosed(1u));

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, cancel_()).Times(0);
  EXPECT_CALL(*subscription, request_(1));

  auto mockSubscriber =
      std::make_shared<StrictMock<MockSubscriber<rsocket::Payload>>>(1000);
  EXPECT_CALL(*mockSubscriber, onSubscribe_(_));
  EXPECT_CALL(*mockSubscriber, onError_(_));
  requester->subscribe(mockSubscriber);

  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = requester.get();
  subscriber->onSubscribe(subscription);

  // Initial request to activate the channel
  subscriber->onNext(Payload());

  ASSERT_FALSE(requester->consumerClosed());
  ASSERT_FALSE(requester->publisherClosed());

  subscriber->onError(std::runtime_error("test"));

  ASSERT_TRUE(requester->consumerClosed());
  ASSERT_TRUE(requester->publisherClosed());
}

TEST(StreamState, ChannelResponderOnError) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto responder = std::make_shared<ChannelResponder>(writer, 1u, 0u);

  EXPECT_CALL(*writer, writeError_(_));
  EXPECT_CALL(*writer, onStreamClosed(1u));
  EXPECT_CALL(*writer, writeRequestN_(_));

  auto mockSubscriber =
      std::make_shared<StrictMock<MockSubscriber<rsocket::Payload>>>();
  EXPECT_CALL(*mockSubscriber, onSubscribe_(_));
  EXPECT_CALL(*mockSubscriber, onError_(_));
  responder->subscribe(mockSubscriber);

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, cancel_()).Times(0);
  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = responder.get();
  subscriber->onSubscribe(subscription);

  ASSERT_FALSE(responder->consumerClosed());
  ASSERT_FALSE(responder->publisherClosed());

  subscriber->onError(std::runtime_error("test"));

  ASSERT_TRUE(responder->consumerClosed());
  ASSERT_TRUE(responder->publisherClosed());
}

TEST(StreamState, ChannelRequesterHandleError) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto requester = std::make_shared<ChannelRequester>(writer, 1u);

  EXPECT_CALL(*writer, writeNewStream_(1u, _, _, _));
  EXPECT_CALL(*writer, writeError_(_)).Times(0);
  EXPECT_CALL(*writer, onStreamClosed(1u)).Times(0);

  auto mockSubscriber =
      std::make_shared<StrictMock<MockSubscriber<rsocket::Payload>>>(1000);
  EXPECT_CALL(*mockSubscriber, onSubscribe_(_));
  EXPECT_CALL(*mockSubscriber, onError_(_));
  requester->subscribe(mockSubscriber);

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, cancel_());
  EXPECT_CALL(*subscription, request_(1));

  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = requester.get();
  subscriber->onSubscribe(subscription);
  // Initial request to activate the channel
  subscriber->onNext(Payload());

  ASSERT_FALSE(requester->consumerClosed());
  ASSERT_FALSE(requester->publisherClosed());

  ConsumerBase* consumer = requester.get();
  consumer->handleError(std::runtime_error("test"));

  ASSERT_TRUE(requester->consumerClosed());
  ASSERT_TRUE(requester->publisherClosed());
}

TEST(StreamState, ChannelResponderHandleError) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto responder = std::make_shared<ChannelResponder>(writer, 1u, 0u);

  EXPECT_CALL(*writer, writeError_(_)).Times(0);
  EXPECT_CALL(*writer, onStreamClosed(1u)).Times(0);
  EXPECT_CALL(*writer, writeRequestN_(_));

  auto mockSubscriber =
      std::make_shared<StrictMock<MockSubscriber<rsocket::Payload>>>();
  EXPECT_CALL(*mockSubscriber, onSubscribe_(_));
  EXPECT_CALL(*mockSubscriber, onError_(_));

  responder->subscribe(mockSubscriber);

  // Initialize the responder
  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, cancel_());
  EXPECT_CALL(*subscription, request_(1)).Times(0);

  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = responder.get();
  subscriber->onSubscribe(subscription);

  ASSERT_FALSE(responder->consumerClosed());
  ASSERT_FALSE(responder->publisherClosed());

  ConsumerBase* consumer = responder.get();
  consumer->handleError(std::runtime_error("test"));

  ASSERT_TRUE(responder->consumerClosed());
  ASSERT_TRUE(responder->publisherClosed());
}

// https://github.com/rsocket/rsocket/blob/master/Protocol.md#cancel-from-requester-responder-terminates
TEST(StreamState, ChannelRequesterCancel) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto requester = std::make_shared<ChannelRequester>(writer, 1u);

  EXPECT_CALL(*writer, writeNewStream_(1u, _, _, _));
  EXPECT_CALL(*writer, writePayload_(_)).Times(2);
  EXPECT_CALL(*writer, writeCancel_(_));
  EXPECT_CALL(*writer, onStreamClosed(1u)).Times(0);

  auto mockSubscriber =
      std::make_shared<StrictMock<MockSubscriber<rsocket::Payload>>>(1000);
  EXPECT_CALL(*mockSubscriber, onSubscribe_(_));
  requester->subscribe(mockSubscriber);

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, cancel_()).Times(0);
  EXPECT_CALL(*subscription, request_(1));
  EXPECT_CALL(*subscription, request_(2));

  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = requester.get();
  subscriber->onSubscribe(subscription);
  // Initial request to activate the channel
  subscriber->onNext(Payload());

  ASSERT_FALSE(requester->consumerClosed());
  ASSERT_FALSE(requester->publisherClosed());

  ConsumerBase* consumer = requester.get();
  consumer->cancel();

  ASSERT_TRUE(requester->consumerClosed());
  ASSERT_FALSE(requester->publisherClosed());

  // Still capable of using the producer side
  StreamStateMachineBase* base = requester.get();
  base->handleRequestN(2u);
  subscriber->onNext(Payload());
  subscriber->onNext(Payload());
}

TEST(StreamState, ChannelResponderCancel) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto responder = std::make_shared<ChannelResponder>(writer, 1u, 0u);

  EXPECT_CALL(*writer, writePayload_(_)).Times(2);
  EXPECT_CALL(*writer, writeCancel_(_));
  EXPECT_CALL(*writer, writeRequestN_(_));

  auto mockSubscriber =
      std::make_shared<StrictMock<MockSubscriber<rsocket::Payload>>>();
  EXPECT_CALL(*mockSubscriber, onSubscribe_(_));

  responder->subscribe(mockSubscriber);

  // Initialize the responder
  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, cancel_()).Times(0);
  EXPECT_CALL(*subscription, request_(2));

  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = responder.get();
  subscriber->onSubscribe(subscription);

  ASSERT_FALSE(responder->consumerClosed());
  ASSERT_FALSE(responder->publisherClosed());

  ConsumerBase* consumer = responder.get();
  consumer->cancel();

  ASSERT_TRUE(responder->consumerClosed());
  ASSERT_FALSE(responder->publisherClosed());

  // Still capable of using the producer side
  StreamStateMachineBase* base = responder.get();
  base->handleRequestN(2u);
  subscriber->onNext(Payload());
  subscriber->onNext(Payload());
}

TEST(StreamState, ChannelRequesterHandleCancel) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto requester = std::make_shared<ChannelRequester>(writer, 1u);

  EXPECT_CALL(*writer, writeNewStream_(1u, _, _, _));
  EXPECT_CALL(*writer, writePayload_(_)).Times(0);
  EXPECT_CALL(*writer, onStreamClosed(1u));

  auto mockSubscriber =
      std::make_shared<StrictMock<MockSubscriber<rsocket::Payload>>>(1000);
  EXPECT_CALL(*mockSubscriber, onSubscribe_(_));
  requester->subscribe(mockSubscriber); // cycle: requester <-> mockSubscriber

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, cancel_());
  EXPECT_CALL(*subscription, request_(1));

  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = requester.get();
  subscriber->onSubscribe(subscription);
  // Initial request to activate the channel
  subscriber->onNext(Payload());

  ASSERT_FALSE(requester->consumerClosed());
  ASSERT_FALSE(requester->publisherClosed());

  ConsumerBase* consumer = requester.get();
  consumer->handleCancel();

  ASSERT_TRUE(requester->publisherClosed());
  ASSERT_FALSE(requester->consumerClosed());

  // As the publisher is closed, this payload will be dropped
  subscriber->onNext(Payload());
  subscriber->onNext(Payload());

  // Break the cycle: requester <-> mockSubscriber
  EXPECT_CALL(*writer, writeCancel_(_));
  auto consumerSubscription = mockSubscriber->subscription();
  consumerSubscription->cancel();
}

TEST(StreamState, ChannelResponderHandleCancel) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto responder = std::make_shared<ChannelResponder>(writer, 1u, 0u);

  EXPECT_CALL(*writer, writePayload_(_)).Times(0);
  EXPECT_CALL(*writer, writeRequestN_(_));
  EXPECT_CALL(*writer, onStreamClosed(1u));

  auto mockSubscriber =
      std::make_shared<StrictMock<MockSubscriber<rsocket::Payload>>>();
  EXPECT_CALL(*mockSubscriber, onSubscribe_(_));
  responder->subscribe(mockSubscriber); // cycle: responder <-> mockSubscriber

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, cancel_());

  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = responder.get();
  subscriber->onSubscribe(subscription);

  ASSERT_FALSE(responder->consumerClosed());
  ASSERT_FALSE(responder->publisherClosed());

  ConsumerBase* consumer = responder.get();
  consumer->handleCancel();

  ASSERT_TRUE(responder->publisherClosed());
  ASSERT_FALSE(responder->consumerClosed());

  // As the publisher is closed, this payload will be dropped
  subscriber->onNext(Payload());
  subscriber->onNext(Payload());

  // Break the cycle: responder <-> mockSubscriber
  EXPECT_CALL(*writer, writeCancel_(_));
  auto consumerSubscription = mockSubscriber->subscription();
  consumerSubscription->cancel();
}
