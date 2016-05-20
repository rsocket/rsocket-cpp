// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

#include "reactivesocket-cpp/src/ConnectionAutomaton.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

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
    Base::logPrefix(LOG(INFO)) << "destroyed";
  }

  /// @{
  /// Publisher<Payload>
  void subscribe(Subscriber<Payload>& subscriber) {
    Base::logPrefix(LOG(INFO)) << "subscribe(" << &subscriber << ")";
    Base::subscribe(subscriber);
  }
  /// @}

  /// @{
  /// Subscription
  void request(size_t n) {
    Base::logPrefix(LOG(INFO)) << "request(" << n << ")";
    Base::request(n);
  }

  void cancel() {
    Base::logPrefix(LOG(INFO)) << "cancel()";
    Base::cancel();
  }
  /// @}

  /// @{
  /// Subscriber<Payload>
  void onSubscribe(Subscription& subscription) {
    Base::logPrefix(LOG(INFO)) << "onSubscribe(" << &subscription << ")";
    Base::onSubscribe(subscription);
  }

  void onNext(Payload payload) {
    Base::logPrefix(LOG(INFO)) << "onNext(<"
                               << payload->computeChainDataLength() << ">)";
    Base::onNext(std::move(payload));
  }

  void onComplete() {
    Base::logPrefix(LOG(INFO)) << "onComplete()";
    Base::onComplete();
  }

  void onError(folly::exception_wrapper ex) {
    Base::logPrefix(LOG(INFO)) << "onComplete(" << ex.class_name() << ")";
    Base::onError(std::move(ex));
  }
  /// @}

  /// @{
  void endStream(StreamCompletionSignal signal) {
    Base::logPrefix(LOG(INFO)) << "endStream(" << signal << ")";
    Base::endStream(signal);
  }
  /// @}

 protected:
  /// @{
  template <typename Frame>
  void onNextFrame(Frame& frame) {
    Base::logPrefix(LOG(INFO)) << frame;
    Base::onNextFrame(frame);
  }

  void onBadFrame() {
    Base::logPrefix(LOG(INFO)) << "bad frame";
    Base::onBadFrame();
  }
  /// @}
};
}
