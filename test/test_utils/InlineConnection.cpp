// Copyright 2004-present Facebook. All Rights Reserved.

#include "InlineConnection.h"
#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include "test/streams/Mocks.h"

namespace rsocket {

InlineConnection::~InlineConnection() {
  this_->instanceTerminated_ = true;
}

void InlineConnection::connectTo(InlineConnection& other) {
  ASSERT_FALSE(other_);
  ASSERT_FALSE(other.other_);
  other.other_ = this_;
  other_ = other.this_;
}

void InlineConnection::setInput(
    std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> inputSink) {
  using namespace ::testing;

  CHECK(other_);
  CHECK(!this_->inputSink_);
  this_->inputSink_ = std::move(inputSink);
  // If `other_->outputSubscription_` is not empty, we can provide the
  // subscription to newly registered `inputSink`.
  // Otherwise, we only record the sink and wait for appropriate sequence of
  // calls to happen on the other end.
  if (other_->outputSubscription_) {
    this_->inputSink_->onSubscribe(other_->outputSubscription_);
    ASSERT_TRUE(!this_->inputSinkCompleted_ || !this_->inputSinkError_);
    // If there are any pending signals, we deliver them now.
    if (this_->inputSinkCompleted_) {
      this_->inputSink_->onComplete();
    } else if (this_->inputSinkError_) {
      this_->inputSink_->onError(this_->inputSinkError_);
    }
  } else {
    // No other signal can precede Subscriber::onSubscribe. Since that one was
    // not delivered to other end's output subscriber, no other signal could be
    // delivered to this subscription.
    ASSERT_FALSE(this_->inputSinkCompleted_);
    ASSERT_FALSE(this_->inputSinkError_);
  }
}

std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
InlineConnection::getOutput() {
  CHECK(other_);

  using namespace ::testing;
  auto outputSink =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();

  // A check point for either of the terminal signals.
  auto* checkpoint = new MockFunction<void()>();

  auto this__ = this_;
  auto other__ = other_;

  Sequence s;
  EXPECT_CALL(*outputSink, onSubscribe_(_))
      .Times(AtMost(1))
      .InSequence(s)
      .WillOnce(
          Invoke([this__, other__](std::shared_ptr<Subscription> subscription) {
            ASSERT_FALSE(this__->outputSubscription_);
            this__->outputSubscription_ = std::move(subscription);
            // If `other_->inputSink_` is not empty, we can provide the
            // subscriber
            // with newly received subscription.
            // Otherwise, we only record the subscription and wait for
            // appropriate
            // sequence of calls to happen on the other end.
            if (other__->inputSink_) {
              other__->inputSink_->onSubscribe(this__->outputSubscription_);
            }
          }));
  EXPECT_CALL(*outputSink, onNext_(_))
      .Times(AnyNumber())
      .InSequence(s)
      .WillRepeatedly(
          Invoke([this__, other__](std::unique_ptr<folly::IOBuf>& frame) {
            ASSERT_TRUE(other__);
            ASSERT_TRUE(other__->outputSubscription_);
            // The handshake must be completed and Subscription::request(n) must
            // be
            // invoked on the other end's input, in order for ::onNext to be
            // called
            // on this end's output.
            ASSERT_TRUE(other__->inputSink_);
            // calling onNext can result in calling terminating signals
            // (onComplete/onError/cancel)
            // and releasing shared_ptrs which may destroy object instances
            // while
            // onNext method is still on the stack
            // we will protect against such bugs by keeping a strong reference
            // to
            // the object while in onNext method
            auto otherInputSink = other__->inputSink_;
            otherInputSink->onNext(std::move(frame));
          }));
  EXPECT_CALL(*outputSink, onComplete_())
      .Times(AtMost(1))
      .InSequence(s)
      .WillOnce(Invoke([this__, other__, checkpoint]() {
        checkpoint->Call();
        ASSERT_TRUE(other__);
        ASSERT_FALSE(this__->inputSinkCompleted_);
        ASSERT_FALSE(this__->inputSinkError_);
        this__->inputSinkCompleted_ = true;
        // We now have two possible situations:
        // * `other_->inputSink_` is not empty, we forward the signal,
        // * otherwise, we only record the signal and wait for appropriate
        //    sequence of calls to happen on the other end.
        if (other__->inputSink_) {
          ASSERT_TRUE(other__->outputSubscription_);
          other__->inputSink_->onComplete();
        }
      }));
  EXPECT_CALL(*outputSink, onError_(_))
      .Times(AtMost(1))
      .InSequence(s)
      .WillOnce(
          Invoke([this__, other__, checkpoint](folly::exception_wrapper ex) {
            checkpoint->Call();
            ASSERT_TRUE(other__);
            ASSERT_TRUE(other__->outputSubscription_);
            ASSERT_FALSE(this__->inputSinkCompleted_);
            ASSERT_FALSE(this__->inputSinkError_);
            this__->inputSinkError_ = ex;
            // We now have two possible situations:
            // * `other_->inputSink_` is not empty, we forward the signal,
            // * otherwise, we only record the signal and wait for appropriate
            //    sequence of calls to happen on the other end.
            if (other__->inputSink_) {
              other__->inputSink_->onError(std::move(ex));
            }
          }));
  EXPECT_CALL(*checkpoint, Call())
      .InSequence(s)
      .WillOnce(Invoke([checkpoint]() {
        Mock::VerifyAndClearExpectations(checkpoint);
        delete checkpoint;
      }));

  return outputSink;
}
}
