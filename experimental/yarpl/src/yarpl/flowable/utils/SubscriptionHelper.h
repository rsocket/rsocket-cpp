// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>

namespace yarpl {
namespace flowable {
namespace internal {
/**
 * Utility functions to help with handling request(long n)
 * since it can be called concurrently and must handle rollover.
 *
 * Since Flowable Subscription must have an int64_t per Subscription, we also
 * leverage it for storing cancellation by setting to INT64_MIN so we don't have
 * to also deal with a separate boolean field per Subscription.
 *
 * These functions are thread-safe and intended to deal with concurrent
 * modification by all working off of std::atomic and using
 * compare_exchange_strong
 */
class SubscriptionHelper {
 public:
  /**
   * Add the new value 'n' to the 'current' atomic_long.
   *
   * Caps the result at INT64_MAX.
   *
   * Adding a negative number does nothing.
   *
   * If 'current' is set to "cancelled" using the magic number INT64_MIN it will
   * not be changed.
   *
   * @param current
   * @param n
   * @return current value after change
   */
  static int64_t addCredits(std::atomic<std::int64_t>* current, int64_t n);

  /**
   * Set 'current' to INT64_MIN as a magic number to represent "cancelled".
   *
   * Return true if this changed to cancelled, or false if it was already
   * cancelled.
   *
   * @param current
   * @return true if changed from not-cancelled to cancelled, false if was
   * already cancelled
   */
  static bool addCancel(std::atomic<std::int64_t>* current);

  /**
   * Consume (remove) credits from the 'current' atomic_long.
   *
   * This MUST only be used to remove credits after emitting a value via onNext.
   *
   * @param current
   * @param n
   * @return current value after change
   */
  static int64_t consumeCredits(std::atomic<std::int64_t>* current, int64_t n);

  /**
   * Whether the current value represents a "cancelled" subscription.
   *
   * @param current
   * @return true if cancelled
   */
  static bool isCancelled(std::atomic<std::int64_t>* current);

  /**
   * If the requested value is MAX so we can ignore flow control.
   * @param current
   * @return true if infinite (max value)
   */
  static bool isInfinite(std::atomic<std::int64_t>* current);
};
}
}
}
