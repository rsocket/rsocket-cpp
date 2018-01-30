// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/flowable/Flowables.h"

namespace yarpl {
namespace flowable {

std::shared_ptr<Flowable<int64_t>> Flowable<>::range(
    int64_t start,
    int64_t count) {
  auto lambda = [start, count, i = start](
                    Subscriber<int64_t>& subscriber,
                    int64_t requested) mutable {
    int64_t end = start + count;

    while (i < end && requested-- > 0) {
      subscriber.onNext(i++);
    }

    if (i >= end) {
      subscriber.onComplete();
    }
  };
  return Flowable<int64_t>::create(std::move(lambda));
}

} // flowable
} // yarpl
