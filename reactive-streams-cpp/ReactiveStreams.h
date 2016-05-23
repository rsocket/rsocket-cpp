#pragma once

#include <exception>

namespace reactivestreams {

template <typename T, typename E>
class Subscriber;
class Subscription;

/// Provides a potentially infinite sequence of elements of type T.
///
/// Rules:
/// TODO(stupaq):
///   * copy from specification for JVM,
///
/// Life cycle considerations:
/// 1. The Publisher is not owned by Subscriber or Subscription.
/// 2. The Publisher can be a temporary object, as it is only used to capture
///   the indirection in creation of the Subscription instance. Publisher's
///   lifetime does not need to extend beyond a lifetime of any of the
///   Subscribers.
template <typename T, typename E = std::exception_ptr>
class Publisher {
 public:
  virtual ~Publisher() = default;

  /// Establishes an abstract subscription between Subscriber and Publisher, by
  /// providing the former with an instance of Subscription.
  ///
  /// Must call Subscriber::onSubscribe synchronously to provide a valid
  /// Subscription.
  ///
  /// Life cycle considerations:
  /// 1. No ownership of the Subscriber is assumed by the Publisher.
  /// 2. The Subsciber pointer MUST remain valid until the Publisher calls
  ///   Subscriber::{onComplete,onError}. See "unsubscribe handshake" for more
  ///   details.
  virtual void subscribe(Subscriber<T, E>& subscriber) = 0;
};

/// Consumes a potentially infinite sequence of elements of type T.
///
/// Rules:
/// TODO(stupaq):
///   * copy from specification for JVM,
///   * remove rule 3. from JVM specification as it would make "unsubscribe
///     handshake" harder to implement in some cases,
///   * add a rule to define "unsubscribe handshake",
///   * add a rule to ensure single terminal signal,
///
/// Life cycle considerations:
/// 1. The Subscriber is not owned by any Publisher, nor it owns a Subscription.
/// 2. Per "unsubscribe handshake", the Publisher MUST always send either
///   ::{onComplete,onError} as the last signal to the Subscriber.
///   This holds even if an unsubscribe sequence is initiated by the Subscriber
///   calling Subscription::cancel. Therefore, it is perfectly possible for a
///   Subscriber to deallocate any resources it holds in ::{onComplete,onError}.
/// 3. Note that no part of ReactiveStreams specification requires the
///   Subscriber to be heap-allocated. However, if it is the case, from the
///   perspective of Publisher-Subscriber interaction, it is valid for the
///   Subscriber to `delete this;` as the last statement in
///   ::{onComplete,onError}.
template <typename T, typename E = std::exception_ptr>
class Subscriber {
 public:
  using ElementType = T;
  using ErrorType = E;

  virtual ~Subscriber() = default;

  /// Called synchronously from Publisher::subscribe to finish the initial
  /// handshake.
  ///
  /// Life cycle considerations:
  /// 1. No ownership of the Subscription is assumed by the Subscriber.
  /// 2. The subscription pointer MUST remain valid until the Subscriber calls
  ///   Subscription::cancel. See "unsubscribe handshake" for more details.
  virtual void onSubscribe(Subscription& subscription) = 0;

  /// Called by or on behalf of Publisher when it wishes to deliver the next
  /// element on a subscription.
  ///
  /// If a Subscriber calls `Subscription::request(1); Subscription::cancel()`,
  /// ::onNext might be invoked after or during the call to
  /// Subscription::cancel. In this case, however, the Publisher MUST eventually
  /// call one of ::{onComplete,onError} to finish the "unsubscribe handshake".
  ///
  /// The method MUST NOT be called after or during an invocation of
  /// ::{onComplete,onError}.
  virtual void onNext(T element) = 0;

  /// Called by or on behalf of Publisher when it wishes to terminate the
  /// abstract subscription gracefully.
  ///
  /// Subscriber pointer passed to the Publisher::subscribe may become invalid
  /// as a result of this call. No other method of the Subscriber can be called
  /// after or during an invocation of ::onComplete.
  virtual void onComplete() = 0;

  /// Called by or on behalf of Publisher when it wishes to terminate the
  /// abstract subscription with an error.
  ///
  /// Subscriber pointer passed to the Publisher::subscribe may become invalid
  /// as a result of this call. No other method of the Subscriber can be called
  /// after or during an invocation of ::onError.
  virtual void onError(E ex) = 0;
};

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
