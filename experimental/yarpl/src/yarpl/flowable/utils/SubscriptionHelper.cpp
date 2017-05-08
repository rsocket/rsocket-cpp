// Copyright 2004-present Facebook. All Rights Reserved.

#include "SubscriptionHelper.h"
#include <atomic>
#include <climits>

namespace yarpl {
namespace flowable {
namespace internal {
int64_t SubscriptionHelper::addCredits(
    std::atomic<std::int64_t>* current,
    int64_t n) {
  for (;;) {
    int64_t r = current->load();
    // if already "infinite"
    if (r == INT64_MAX) {
      return INT64_MAX;
    }
    // if already "cancelled"
    if (r == INT64_MIN) {
      return INT64_MIN;
    }
    if (n <= 0) {
      // do nothing, return existing unmodified value
      return r;
    }

    if (r > INT64_MAX - n) {
      // will overflow
      current->store(INT64_MAX);
      return INT64_MAX;
    }

    int64_t u = r + n;
    // set the new number
    if (current->compare_exchange_strong(r, u)) {
      return u;
    }
    // if failed to set (concurrent modification) loop and try again
  }
}

bool SubscriptionHelper::addCancel(std::atomic<std::int64_t>* current) {
  for (;;) {
    int64_t r = current->load();
    if (r == INT64_MIN) {
      // already cancelled
      return false;
    }
    // try cancelling
    if (current->compare_exchange_strong(r, INT64_MIN)) {
      return true;
    }
    // if failed to set (concurrent modification) loop and try again
  }
}

int64_t SubscriptionHelper::consumeCredits(
    std::atomic<std::int64_t>* current,
    int64_t n) {
  for (;;) {
    int64_t r = current->load();
    if (n <= 0) {
      // do nothing, return existing unmodified value
      return r;
    }
    if (r < n) {
      // bad usage somewhere ... be resilient, just set to r
      n = r;
    }

    int64_t u = r - n;

    // set the new number
    if (current->compare_exchange_strong(r, u)) {
      return u;
    }
    // if failed to set (concurrent modification) loop and try again
  }
}

bool SubscriptionHelper::isCancelled(std::atomic<std::int64_t>* current) {
  return current->load() == INT64_MIN;
}

bool SubscriptionHelper::isInfinite(std::atomic<std::int64_t>* current) {
  return current->load() == INT64_MAX;
}
}
}
}