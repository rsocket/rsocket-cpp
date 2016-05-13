// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>
#include <type_traits>

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>

#include "reactivesocket-cpp/src/ConnectionAutomaton.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace lithium {
namespace reactivesocket {

class IntrusiveDeleter;

/// Handles automatic memory management for entire chain of mixins by
/// piggy-backing on terminal signals of a Subscriber, Subscription and
/// AbstractStreamAutomaton.
///
/// The mixin decrements object's reference count provided by an
/// IntrusiveDeleter after receiving a terminal signal. Since this may result in
/// `this` being deleted, the mixin should "wrap" terminal signals delivered to
/// the class, to ensure that signal handling code does not access `this` after
/// the reference count has been decremented.
///
/// Uses lazy method instantiantiation trick, see LoggingMixin.
template <typename Base>
class MemoryMixin : public Base {
  static_assert(
      std::is_base_of<IntrusiveDeleter, Base>::value,
      "Base must be a descendant of IntrusiveDeleter");

 public:
  using Base::Base;

  ~MemoryMixin() {}

  /// @{
  /// Publisher<Payload>
  void subscribe(Subscriber<Payload>& subscriber) {
    Base::incrementRefCount();
    Base::subscribe(subscriber);
  }
  /// @}

  /// @{
  /// Subscription
  void request(size_t n) {
    Base::request(n);
  }

  void cancel() {
    Base::cancel();
    Base::decrementRefCount();
  }
  /// @}

  /// @{
  /// Subscriber<Payload>
  void onSubscribe(Subscription& subscription) {
    Base::incrementRefCount();
    Base::onSubscribe(subscription);
  }

  void onNext(Payload payload) {
    Base::onNext(std::move(payload));
  }

  void onComplete() {
    Base::onComplete();
    Base::decrementRefCount();
  }

  void onError(folly::exception_wrapper ex) {
    Base::onError(std::move(ex));
    Base::decrementRefCount();
  }
  /// @}

  /// @{
  void endStream(StreamCompletionSignal signal) {
    Base::endStream(signal);
    Base::decrementRefCount();
  }
  /// @}

 protected:
  /// @{
  template <typename Frame>
  void onNextFrame(Frame& frame) {
    Base::onNextFrame(frame);
  }

  void onBadFrame() {
    Base::onBadFrame();
  }
  /// @}

  std::ostream& logPrefix(std::ostream& os) {
    return os << "MemoryMixin(" << &this->connection_ << ", " << this->streamId_
              << "): ";
  }
};
}
}
