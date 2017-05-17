// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <reactive-streams/ReactiveStreams.h>

namespace folly {
class exception_wrapper;
}

namespace rsocket {

template <typename T>
using Publisher = reactivestreams::Publisher<T, folly::exception_wrapper>;
template <typename T>
using Subscriber = reactivestreams::Subscriber<T, folly::exception_wrapper>;
using Subscription = reactivestreams::Subscription;

} // namespace rsocket
