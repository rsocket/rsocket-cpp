// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "test/streams/Mocks.h"

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
}
