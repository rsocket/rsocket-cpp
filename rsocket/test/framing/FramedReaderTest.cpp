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

#include <gtest/gtest.h>

#include "rsocket/framing/FramedReader.h"
#include "rsocket/test/test_utils/MockDuplexConnection.h"

using namespace rsocket;
using namespace testing;
using namespace yarpl::mocks;

TEST(FramedReader, TinyFrame) {
  auto version = std::make_shared<ProtocolVersion>(ProtocolVersion::Latest);
  auto reader = std::make_shared<FramedReader>(version);

  // Not using hex string-literal as std::string ctor hits '\x00' and stops
  // reading.
  auto buf = folly::IOBuf::createCombined(4);
  buf->append(4);
  buf->writableData()[0] = '\x00';
  buf->writableData()[1] = '\x00';
  buf->writableData()[2] = '\x00';
  buf->writableData()[3] = '\x02';

  reader->onSubscribe(yarpl::flowable::Subscription::create());
  reader->onNext(std::move(buf));

  auto subscriber = std::make_shared<
      StrictMock<MockSubscriber<std::unique_ptr<folly::IOBuf>>>>();
  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onError_(_));

  reader->setInput(subscriber);
  subscriber->awaitTerminalEvent();
  reader->onComplete();
}

TEST(FramedReader, CantDetectVersion) {
  auto version = std::make_shared<ProtocolVersion>(ProtocolVersion::Unknown);
  auto reader = std::make_shared<FramedReader>(version);

  auto buf = folly::IOBuf::copyBuffer("ABCDEFGHIJKLMNOP");

  reader->onSubscribe(yarpl::flowable::Subscription::create());
  reader->onNext(std::move(buf));

  auto subscriber = std::make_shared<
      StrictMock<MockSubscriber<std::unique_ptr<folly::IOBuf>>>>();
  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onError_(_));

  reader->setInput(subscriber);
  subscriber->awaitTerminalEvent();
  reader->onComplete();
}

TEST(FramedReader, SubscriberCompleteAfterError) {
  auto version = std::make_shared<ProtocolVersion>(ProtocolVersion::Latest);
  auto reader = std::make_shared<FramedReader>(version);

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, request_(_));
  EXPECT_CALL(*subscription, cancel_());

  reader->onSubscribe(subscription);

  auto subscriber = std::make_shared<
      StrictMock<MockSubscriber<std::unique_ptr<folly::IOBuf>>>>();
  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onError_(_))
      .WillOnce(Invoke([](folly::exception_wrapper ew) {
        EXPECT_EQ(ew.get_exception()->what(), std::string{"Oops"});
      }));

  reader->setInput(subscriber);
  reader->error("Oops");
  reader->onComplete();
}

TEST(FramedReader, SubscriberErrorAfterError) {
  auto version = std::make_shared<ProtocolVersion>(ProtocolVersion::Latest);
  auto reader = std::make_shared<FramedReader>(version);

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, request_(_));
  EXPECT_CALL(*subscription, cancel_());

  reader->onSubscribe(subscription);

  auto subscriber = std::make_shared<
      StrictMock<MockSubscriber<std::unique_ptr<folly::IOBuf>>>>();
  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onError_(_))
      .WillOnce(Invoke([](folly::exception_wrapper ew) {
        EXPECT_EQ(ew.get_exception()->what(), std::string{"Oops"});
      }));

  reader->setInput(subscriber);
  reader->error("Oops");
  reader->onError(std::runtime_error{"Not oops"});
}
