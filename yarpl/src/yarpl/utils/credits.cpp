// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/utils/credits.h"

#include <atomic>
#include <limits>

namespace yarpl {
namespace credits {

int64_t add(std::atomic<std::int64_t>* current, int64_t n) {
  for (;;) {
    auto r = current->load();
    // if already "infinite"
    if (r == kNoFlowControl) {
      return kNoFlowControl;
    }
    // if already "cancelled"
    if (r == kCanceled) {
      return kCanceled;
    }
    if (n <= 0) {
      // do nothing, return existing unmodified value
      return r;
    }

    if (r > kNoFlowControl - n) {
      // will overflow
      current->store(kNoFlowControl);
      return kNoFlowControl;
    }

    auto u = r + n;
    // set the new number
    if (current->compare_exchange_strong(r, u)) {
      return u;
    }
    // if failed to set (concurrent modification) loop and try again
  }
}

int64_t add(int64_t current, int64_t n) {
  if (n <= 0) {
    return current;
  }
  if (current == kCanceled) {
    return kCanceled;
  }
  if (current > kNoFlowControl - n) {
    return kNoFlowControl;
  }
  return current + n;
}

bool cancel(std::atomic<std::int64_t>* current) {
  for (;;) {
    auto r = current->load();
    if (r == kCanceled) {
      // already cancelled
      return false;
    }
    // try cancelling
    if (current->compare_exchange_strong(r, kCanceled)) {
      return true;
    }
    // if failed to set (concurrent modification) loop and try again
  }
}

int64_t consume(std::atomic<std::int64_t>* current, int64_t n) {
  for (;;) {
    auto r = current->load();
    if (n <= 0) {
      // do nothing, return existing unmodified value
      return r;
    }
    if (r < n) {
      // bad usage somewhere ... be resilient, just set to r
      n = r;
    }

    auto u = r - n;

    // set the new number
    if (current->compare_exchange_strong(r, u)) {
      return u;
    }
    // if failed to set (concurrent modification) loop and try again
  }
}

bool isCancelled(std::atomic<std::int64_t>* current) {
  return current->load() == kCanceled;
}

bool isInfinite(std::atomic<std::int64_t>* current) {
  return current->load() == kNoFlowControl;
}

} // namespace credits
} // namespace yarpl
