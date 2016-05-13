// Copyright 2004-present Facebook. All Rights Reserved.


#pragma once

#include "reactive-streams-cpp/Mocks.h"

namespace folly {
class exception_wrapper;
}

/// This header defines aliases to the mocks provided by the ReactiveStream
/// specification, which replace std::exception_ptr with more efficient
/// folly::exception_wrapper.
namespace lithium {
namespace reactivesocket {

template <typename T>
using UnmanagedMockPublisher =
    reactivestreams::UnmanagedMockPublisher<T, folly::exception_wrapper>;
template <typename T>
using UnmanagedMockSubscriber =
    reactivestreams::UnmanagedMockSubscriber<T, folly::exception_wrapper>;
using UnmanagedMockSubscription = reactivestreams::UnmanagedMockSubscription;

template <typename T>
using MockSubscriber =
    reactivestreams::MockSubscriber<T, folly::exception_wrapper>;
using MockSubscription = reactivestreams::MockSubscription;

template <typename T>
MockSubscriber<T>& makeMockSubscriber() {
  return reactivestreams::makeMockSubscriber<T, folly::exception_wrapper>();
}

inline MockSubscription& makeMockSubscription() {
  return reactivestreams::makeMockSubscription();
}
}
}
