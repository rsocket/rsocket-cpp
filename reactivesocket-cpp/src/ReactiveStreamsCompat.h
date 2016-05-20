// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "reactive-streams-cpp/Publisher.h"
#include "reactive-streams-cpp/Subscriber.h"
#include "reactive-streams-cpp/Subscription.h"

namespace folly {
class exception_wrapper;
}

/// This header defines aliases to the interfaces defined in the ReactiveStream
/// specification, which replace std::exception_ptr with more efficient
/// folly::exception_wrapper.
namespace reactivestreams {

template <typename S>
class SubscriberPtr;

template <typename S>
class SubscriptionPtr;

} // namespace reactivestreams

namespace reactivesocket {

template <typename T>
using Publisher = reactivestreams::Publisher<T, folly::exception_wrapper>;
template <typename T>
using Subscriber = reactivestreams::Subscriber<T, folly::exception_wrapper>;
using Subscription = reactivestreams::Subscription;

template <typename S>
using SubscriberPtr = reactivestreams::SubscriberPtr<S>;

template <typename S>
using SubscriptionPtr = reactivestreams::SubscriptionPtr<S>;

} // namespace reactivesocket
