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

#include "rsocket/statemachine/ChannelRequester.h"
#include "rsocket/test/test_utils/MockStreamsWriter.h"

using namespace rsocket;
using namespace testing;

TEST(StreamsWriterTest, DelegateMock) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto& impl = writer->delegateToImpl();
  EXPECT_CALL(impl, outputFrame_(_));
  EXPECT_CALL(impl, shouldQueue()).WillOnce(Return(false));
  EXPECT_CALL(*writer, writeNewStream_(_, _, _, _));

  auto requester = std::make_shared<ChannelRequester>(writer, 1u);
  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = requester.get();
  subscriber->onSubscribe(yarpl::flowable::Subscription::create());
  subscriber->onNext(Payload());
}

TEST(StreamsWriterTest, NewStreamsMockWriterImpl) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriterImpl>>();
  EXPECT_CALL(*writer, outputFrame_(_));
  EXPECT_CALL(*writer, shouldQueue()).WillOnce(Return(false));

  auto requester = std::make_shared<ChannelRequester>(writer, 1u);
  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = requester.get();
  subscriber->onSubscribe(yarpl::flowable::Subscription::create());
  subscriber->onNext(Payload());
}

TEST(StreamsWriterTest, QueueFrames) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto& impl = writer->delegateToImpl();
  impl.shouldQueue_ = true;

  EXPECT_CALL(impl, outputFrame_(_)).Times(0);
  EXPECT_CALL(impl, shouldQueue()).WillOnce(Return(true));
  EXPECT_CALL(*writer, writeNewStream_(_, _, _, _));

  auto requester = std::make_shared<ChannelRequester>(writer, 1u);
  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = requester.get();
  subscriber->onSubscribe(yarpl::flowable::Subscription::create());
  subscriber->onNext(Payload());
}

TEST(StreamsWriterTest, FlushQueuedFrames) {
  auto writer = std::make_shared<StrictMock<MockStreamsWriter>>();
  auto& impl = writer->delegateToImpl();
  impl.shouldQueue_ = true;

  EXPECT_CALL(impl, outputFrame_(_)).Times(1);
  EXPECT_CALL(impl, shouldQueue()).Times(3);
  EXPECT_CALL(*writer, writeNewStream_(_, _, _, _));

  auto requester = std::make_shared<ChannelRequester>(writer, 1u);
  yarpl::flowable::Subscriber<rsocket::Payload>* subscriber = requester.get();
  subscriber->onSubscribe(yarpl::flowable::Subscription::create());
  subscriber->onNext(Payload());

  // Will queue again
  impl.sendPendingFrames();

  // Now send them actually
  impl.shouldQueue_ = false;
  impl.sendPendingFrames();
  // it will not send the pending frames twice
  impl.sendPendingFrames();
}
