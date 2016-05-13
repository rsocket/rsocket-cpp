// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <exception>

namespace lithium {
namespace reactivestreams {

class Subscription;

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
}
}
