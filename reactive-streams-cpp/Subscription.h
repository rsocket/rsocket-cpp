// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>

namespace lithium {
namespace reactivestreams {

/// Represents a connection between Publisher and Subscriber established by an
/// invocation of Subscriber::onSubscribe performed by the Publisher.
///
/// Rules:
/// TODO(stupaq):
///   * copy from specification for JVM,
///   * add a rule to define "unsubscribe handshake",
///   * add rule to ensure single terminal signal,
///
/// Life cycle considerations:
/// 1. Subscription is borrowed to the Subscriber in Subscriber::onSubscribe.
/// 2. Per "unsubscribe handshake", the Subscriber MUST invoke ::cancel as the
///   last signal to the Publisher. This holds even if an unsubscribe sequence
///   is initiated by the Publisher calling Subscriber::{onComplete, onError}.
///   Therefore, it is valid for the Subscription to free any resources it holds
///   in ::cancel.
/// 3. Note that no part of ReactiveStreams specification requires the
///   Subscription to be heap-allocated. However, if it is the case, it is
///   valid from the perspective of Subscriber-Subscription interaction to
///   `delete this;` as the last statement in ::cancel.
///
/// Note that it is valid, from the perspective of the ReactiveStreams
/// specification, for a Subscription and a Publisher to be the same object,
/// as long as the Publisher has only one Subscriber.
class Subscription {
 public:
  virtual ~Subscription() = default;

  /// Called by the Subscriber to communicate readiness to accept `n` more
  /// elements of the stream.
  ///
  /// It is legal for Publisher to call Subscriber::onNext synchronously from
  /// ::request. Similarily, it is possible for Subscriber to invoke
  /// ::request from Subscriber::onNext. To break the symmetry and prevent
  /// unbounded stack growth, implementation of ::request is required to place
  /// an upper bound on potential synchronous recursion described above.
  ///
  /// Such upper bound can be ensured by applying the following pattern:
  ///
  ///   AllowanceSemaphore allowance_;
  ///   void ...::request(size_t n) {
  ///     if (allowance_.release(n) > 0) {
  ///       // Either we entered ::reqeust for the second time in the same
  ///       // recursive chain, or we don't have elements to deliver to the
  ///       // subscriber. If we had them, they would be delivered eagerly
  ///       // and we would observe null allowance.
  ///       return;
  ///     }
  ///     // Attempt to push (via Subscriber::onNext) any pending elements that
  ///     // were queued up due to null allowance. This could involve a call to
  ///     // ::request of a subscription that feeds this Publisher with data.
  ///   }
  ///
  virtual void request(size_t n) = 0;

  /// Called by the Subscriber to communicate intention to terminate the
  /// abstract subscription.
  ///
  /// Subscription pointer passed to the Subscriber::onSubscribe may become
  /// invalid as a result of this call. No other method of the Subscription can
  /// be called after or during an invocation of ::cancel.
  ///
  /// The Subscriber calling this method will receive
  /// Subscribe::{onComplete,onError} as an acknowledgement that the abstract
  /// subscription has been terminated and no other signal will be sent to it.
  virtual void cancel() = 0;
};
}
}
