// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/InlineConnection.h"

#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/ReactiveStreamsCompat.h"
#include "test/ReactiveStreamsMocksCompat.h"

namespace reactivesocket {

void InlineConnection::connectTo(InlineConnection& other) {
  ASSERT_FALSE(other_);
  ASSERT_FALSE(other.other_);
  other.other_ = this;
  other_ = &other;
}

void InlineConnection::setInput(
    std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> inputSink) {
  using namespace ::testing;

  ASSERT_TRUE(other_);
  ASSERT_FALSE(inputSink_);
  inputSink_ = std::move(inputSink);
  // If `other_->outputSubscription_` is not empty, we can provide the
  // subscription to newly registered `inputSink`.
  // Otherwise, we only record the sink and wait for appropriate sequence of
  // calls to happen on the other end.
  if (other_->outputSubscription_) {
    inputSink_->onSubscribe(other_->outputSubscription_);
    ASSERT_TRUE(!inputSinkCompleted_ || !inputSinkError_);
    // If there are any pending signals, we deliver them now.
    if (inputSinkCompleted_) {
      inputSink_->onComplete();
    } else if (inputSinkError_) {
      inputSink_->onError(inputSinkError_);
    }
  } else {
    // No other signal can precede Subscriber::onSubscribe. Since that one was
    // not delivered to other end's output subscriber, no other signal could be
    // delivered to this subscription.
    ASSERT_FALSE(inputSinkCompleted_);
    ASSERT_FALSE(inputSinkError_);
  }
}

std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
InlineConnection::getOutput() {
  using namespace ::testing;

  auto outputSink = makeMockSubscriber<std::unique_ptr<folly::IOBuf>>();
  // A check point for either of the terminal signals.
  auto* checkpoint = new MockFunction<void()>();

  // TODO: enable Sequence
  // Sequence s;
  EXPECT_CALL(*outputSink, onSubscribe_(_))
      .Times(AtMost(1))
      //      .InSequence(s)
      .WillOnce(Invoke([this](std::shared_ptr<Subscription> subscription) {
        ASSERT_FALSE(outputSubscription_);
        outputSubscription_ = std::move(subscription);
        // If `other_->inputSink_` is not empty, we can provide the subscriber
        // with newly received subscription.
        // Otherwise, we only record the subscription and wait for appropriate
        // sequence of calls to happen on the other end.
        if (other_->inputSink_) {
          other_->inputSink_->onSubscribe(outputSubscription_);
        }
      }));
  EXPECT_CALL(*outputSink, onNext_(_))
      .Times(AnyNumber())
      //      .InSequence(s)
      .WillRepeatedly(Invoke([this](std::unique_ptr<folly::IOBuf>& frame) {
        ASSERT_TRUE(other_);
        ASSERT_TRUE(other_->outputSubscription_);
        // The handshake must be completed and Subscription::request(n) must be
        // invoked on the other end's input, in order for ::onNext to be called
        // on this end's output.
        ASSERT_TRUE(other_->inputSink_);
        // calling onNext can result in calling terminating signals
        // (onComplete/onError/cancel)
        // and releasing shared_ptrs which may destroy object instances while
        // onNext method is still on the stack
        // we will protect against such bugs by keeping a strong reference to
        // the object while in onNext method
        auto otherInputSink = other_->inputSink_;
        otherInputSink->onNext(std::move(frame));
      }));
  EXPECT_CALL(*outputSink, onComplete_())
      .Times(AtMost(1))
      //      .InSequence(s)
      .WillOnce(Invoke([this, checkpoint]() {
        checkpoint->Call();
        ASSERT_TRUE(other_);
        ASSERT_FALSE(inputSinkCompleted_);
        ASSERT_FALSE(inputSinkError_);
        inputSinkCompleted_ = true;
        // We now have two possible situations:
        // * `other_->inputSink_` is not empty, we forward the signal,
        // * otherwise, we only record the signal and wait for appropriate
        //    sequence of calls to happen on the other end.
        if (other_->inputSink_) {
          ASSERT_TRUE(other_->outputSubscription_);
          other_->inputSink_->onComplete();
        }
      }));
  EXPECT_CALL(*outputSink, onError_(_))
      .Times(AtMost(1))
      //      .InSequence(s)
      .WillOnce(Invoke([this, checkpoint](folly::exception_wrapper ex) {
        checkpoint->Call();
        ASSERT_TRUE(other_);
        ASSERT_TRUE(other_->outputSubscription_);
        ASSERT_FALSE(inputSinkCompleted_);
        ASSERT_FALSE(inputSinkError_);
        inputSinkError_ = ex;
        // We now have two possible situations:
        // * `other_->inputSink_` is not empty, we forward the signal,
        // * otherwise, we only record the signal and wait for appropriate
        //    sequence of calls to happen on the other end.
        if (other_->inputSink_) {
          other_->inputSink_->onError(std::move(ex));
        }
      }));
  EXPECT_CALL(*checkpoint, Call())
      //      .InSequence(s)
      .WillOnce(Invoke([checkpoint]() { delete checkpoint; }));

  return outputSink;
}
}
