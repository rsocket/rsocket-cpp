// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "streams/Mocks.h"

namespace folly {
class exception_wrapper;
}

/// This header defines aliases to the mocks provided by the ReactiveStream
/// specification, which replace std::exception_ptr with more efficient
/// folly::exception_wrapper.
namespace reactivesocket {

template <typename T>
using MockSubscriber =
    reactivestreams::MockSubscriber<T, folly::exception_wrapper>;
using MockSubscription = reactivestreams::MockSubscription;

template <typename T>
std::shared_ptr<MockSubscriber<T>> makeMockSubscriber() {
  return reactivestreams::makeMockSubscriber<T, folly::exception_wrapper>();
}

inline std::shared_ptr<MockSubscription> makeMockSubscription() {
  return reactivestreams::makeMockSubscription();
}
}
