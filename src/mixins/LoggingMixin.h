// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

#include "src/ConnectionAutomaton.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

/// Logs every frame or signal received by the Base.
///
/// The mixin cleverly utilises the fact that non-template methods of a template
/// class are instantiated lazily -- their bodies are not instantiated unless
/// required. In this way, the mixin can adapt its own interface to the
/// interface of the base class automatically: one can use it if a Base
/// implements any subset of Subscriber, Subscription and
/// AbstractStreamAutomaton API (e.g. half of Subscription and a half of
/// Subscriber is fine).
/// http://www.codesynthesis.com/~boris/blog/2012/03/20/delaying-function-signature-instantiation-cxx11/
///
/// The mixin behaves "as expected". If Base doesn't implement cancel() any
/// attempt to instantiate LoggingMixin<Base>::cancel will fail.
template <typename Base>
class LoggingMixin : public Base {
 public:
  using Base::Base;

  ~LoggingMixin() {
    VLOG(6) << *this << "destroyed";
  }

  /// @{
  /// Publisher<Payload>
  void subscribe(std::shared_ptr<Subscriber<Payload>> subscriber) {
    VLOG(6) << *this << "subscribe(" << subscriber.get() << ")";
    Base::subscribe(std::move(subscriber));
  }
  /// @}

  /// @{
  /// Subscription
  void request(size_t n) {
    VLOG(7) << *this << "request(" << n << ")";
    Base::request(n);
  }

  void cancel() {
    VLOG(6) << *this << "cancel()";
    Base::cancel();
  }
  /// @}

  /// @{
  /// Subscriber<Payload>
  void onSubscribe(std::shared_ptr<Subscription> subscription) {
    VLOG(6) << *this << "onSubscribe(" << subscription.get() << ")";
    Base::onSubscribe(std::move(subscription));
  }

  void onNext(Payload payload) {
    VLOG(8) << *this << "onNext(" << payload << ")";
    Base::onNext(std::move(payload));
  }

  void onNext(Payload payload, FrameFlags flags) {
    VLOG(8) << *this << "onNext(" << payload << ", " << flags << ")";
    Base::onNext(std::move(payload), flags);
  }

  void onComplete() {
    VLOG(6) << *this << "onComplete()";
    Base::onComplete();
  }

  void onError(folly::exception_wrapper ex) {
    Base::logPrefix(LOG(INFO)) << "onError(" << ex.class_name() << ")";
    Base::onError(std::move(ex));
  }
  /// @}

  /// @{
  void endStream(StreamCompletionSignal signal) {
    VLOG(6) << *this << "endStream(" << signal << ")";
    Base::endStream(signal);
  }
  /// @}

  friend std::ostream& operator<<(std::ostream& os, Base& base) {
    return base.logPrefix(os);
  }

 protected:
  /// @{
  template <typename Frame>
  void onNextFrame(Frame&& frame) {
    VLOG(8) << *this << frame;
    Base::onNextFrame(std::move(frame));
  }

  void onBadFrame() {
    Base::logPrefix(LOG(INFO)) << "bad frame";
    Base::onBadFrame();
  }
  /// @}
};
}
