// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>
#include <type_traits>

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>

#include "src/ConnectionAutomaton.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/IntrusiveDeleter.h"

namespace reactivesocket {

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
/// The request/cancel or onSubscribe/onNext/onComplete methods will be
/// implicitly specified as virtual, depending on whether the Base class
/// is implementing the (virtual) methods of the
/// Subscription or the Subscriber interface.
template <typename Base, typename T = Payload>
class MemoryMixin : public Base {
  static_assert(
      std::is_base_of<IntrusiveDeleter, Base>::value,
      "Base must be a descendant of IntrusiveDeleter");

 public:
  using Base::Base;

  ~MemoryMixin() {}

  /// @{
  /// Publisher<T>
  void subscribe(Subscriber<T>& subscriber) {
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
  /// Subscriber<T>
  void onSubscribe(Subscription& subscription) {
    Base::incrementRefCount();
    Base::onSubscribe(subscription);
  }

  void onNext(T payload) {
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
  void onNextFrame(Frame&& frame) {
    Base::onNextFrame(std::move(frame));
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

namespace details {

template <typename Base>
class WithIntrusiveDeleter {
  class BaseWithIntrusiveDeleter : public IntrusiveDeleter, public Base {
   public:
    using Base::Base;
  };

 public:
  using T = typename std::conditional<
      std::is_base_of<IntrusiveDeleter, Base>::value,
      Base,
      BaseWithIntrusiveDeleter>::type;
};

} // namespace details

template <typename Base, typename T = Payload, typename... TArgs>
Base& createManagedInstance(TArgs&&... args) {
  auto* instance =
      new MemoryMixin<typename details::WithIntrusiveDeleter<Base>::T, T>(
          std::forward<TArgs>(args)...);
  return *instance;
}
}
