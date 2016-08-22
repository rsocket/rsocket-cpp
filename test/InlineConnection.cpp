// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/InlineConnection.h"

#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/ReactiveStreamsCompat.h"
#include "test/ReactiveStreamsMocksCompat.h"

namespace reactivesocket {

InlineConnection::InlineConnection()
    : other_(nullptr),
      inputSink_(nullptr),
      inputSinkCompleted_(false),
      outputSubscription_(nullptr) {}

InlineConnection::~InlineConnection() {}

void InlineConnection::connectTo(
    InlineConnection& other,
    bool expectSetupFrame) {
  ASSERT_FALSE(other_);
  ASSERT_FALSE(other.other_);
  other.other_ = this;
  other_ = &other;
  // this could just be true for the client, false for the server.  However
  // InlineConnection is below the
  // ReactiveSocket layer so any tests that just use DuplexConnection may break
  // this
  expectSetupFrame_ = expectSetupFrame;
}

void InlineConnection::setInput(
    Subscriber<std::unique_ptr<folly::IOBuf>>& inputSink) {
  using namespace ::testing;

  ASSERT_TRUE(other_);
  ASSERT_FALSE(inputSink_);
  inputSink_ = &inputSink;
  auto outputSubscription = other_->outputSubscription_;
  // If `other_->outputSubscription_` is not empty, we can provide the
  // subscription to newly registered `inputSink`.
  // Otherwise, we only record the sink and wait for appropriate sequence of
  // calls to happen on the other end.
  if (outputSubscription) {
    inputSink.onSubscribe(*outputSubscription);
    ASSERT_TRUE(!inputSinkCompleted_ || !inputSinkError_);
    // If there are any pending signals, we deliver them now.
    if (inputSinkCompleted_) {
      inputSink.onComplete();
    } else if (inputSinkError_) {
      inputSink.onError(inputSinkError_);
    }
  } else {
    // No other signal can preced Subscriber::onSubscribe. Since that one was
    // not delivered to other end's output subscriber, no other signal could be
    // delivered to this subscription.
    ASSERT_FALSE(inputSinkCompleted_);
    ASSERT_FALSE(inputSinkError_);
  }
}

Subscriber<std::unique_ptr<folly::IOBuf>>& InlineConnection::getOutput() {
  using namespace ::testing;

  auto& outputSink = makeMockSubscriber<std::unique_ptr<folly::IOBuf>>();
  // A check point for either of the terminal signals.
  auto checkpoint = new MockFunction<void()>();

  Sequence s;
  EXPECT_CALL(outputSink, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([this](Subscription* subscription) {
        ASSERT_FALSE(outputSubscription_);
        outputSubscription_ = subscription;
        auto inputSink = other_->inputSink_;
        // If `other_->inputSink_` is not empty, we can provide the subscriber
        // with newly received subscription.
        // Otherwise, we only record the subscription and wait for appropriate
        // sequence of calls to happen on the other end.
        if (inputSink) {
          inputSink->onSubscribe(*outputSubscription_);
        }
      }));
  // SETUP
  if (expectSetupFrame_) {
    EXPECT_CALL(outputSink, onNext_(_))
        .Times(1)
        .InSequence(s)
        .WillRepeatedly(Invoke([this](std::unique_ptr<folly::IOBuf>& frame) {
          ASSERT_TRUE(other_);
          ASSERT_FALSE(other_->outputSubscription_);
          auto inputSink = other_->inputSink_;
          ASSERT_FALSE(inputSink);
        }));
  }
  EXPECT_CALL(outputSink, onNext_(_))
      .Times(AnyNumber())
      .InSequence(s)
      .WillRepeatedly(Invoke([this](std::unique_ptr<folly::IOBuf>& frame) {
        ASSERT_TRUE(other_);
        ASSERT_TRUE(other_->outputSubscription_);
        auto inputSink = other_->inputSink_;
        // The handshake must be completed and Subscription::request(n) must be
        // invoked on the other end's input, in order for ::onNext to be called
        // on this end's output.
        ASSERT_TRUE(inputSink);
        inputSink->onNext(std::move(frame));
      }));
  EXPECT_CALL(outputSink, onComplete_())
      .Times(AtMost(1))
      .InSequence(s)
      .WillOnce(Invoke([this, checkpoint]() {
        checkpoint->Call();
        ASSERT_TRUE(other_);
        ASSERT_TRUE(other_->outputSubscription_);
        ASSERT_FALSE(inputSinkCompleted_);
        ASSERT_FALSE(inputSinkError_);
        inputSinkCompleted_ = true;
        auto inputSink = other_->inputSink_;
        // We now have two possible situations:
        // * `other_->inputSink_` is not empty, we forward the signal,
        // * otherwise, we only record the signal and wait for appropriate
        //    sequence of calls to happen on the other end.
        if (inputSink) {
          inputSink->onComplete();
        }
      }));
  EXPECT_CALL(outputSink, onError_(_))
      .Times(AtMost(1))
      .InSequence(s)
      .WillOnce(Invoke([this, checkpoint](folly::exception_wrapper ex) {
        checkpoint->Call();
        ASSERT_TRUE(other_);
        ASSERT_TRUE(other_->outputSubscription_);
        ASSERT_FALSE(inputSinkCompleted_);
        ASSERT_FALSE(inputSinkError_);
        inputSinkError_ = ex;
        auto inputSink = other_->inputSink_;
        // We now have two possible situations:
        // * `other_->inputSink_` is not empty, we forward the signal,
        // * otherwise, we only record the signal and wait for appropriate
        //    sequence of calls to happen on the other end.
        if (inputSink) {
          inputSink->onError(std::move(ex));
        }
      }));
  EXPECT_CALL(*checkpoint, Call())
      .InSequence(s)
      .WillOnce(Invoke([checkpoint]() { delete checkpoint; }));

  return outputSink;
}
}
