// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace yarpl {
namespace credits {

constexpr int64_t kCanceled{std::numeric_limits<int64_t>::min()};
constexpr int64_t kNoFlowControl{std::numeric_limits<int64_t>::max()};

/**
 * Utility functions to help with handling request(int64_t n) since it can be
 * called concurrently and must handle rollover.
 *
 * Since Flowable Subscription must have an int64_t per Subscription, we also
 * leverage it for storing cancellation by setting to INT64_MIN so we don't have
 * to also deal with a separate boolean field per Subscription.
 *
 * These functions are thread-safe and intended to deal with concurrent
 * modification by all working off of std::atomic and using
 * compare_exchange_strong.
 */

/**
 * Add the new value 'n' to the 'current' atomic<int64_t>.
 *
 * Caps the result at INT64_MAX.
 *
 * Adding a negative number does nothing.
 *
 * If 'current' is set to "cancelled" using the magic number INT64_MIN it will
 * not be changed.
 */
int64_t add(std::atomic<int64_t>*, int64_t);

/**
 * Version of add that works for non-atomic integers.
 */
int64_t add(int64_t, int64_t);

/**
 * Set 'current' to INT64_MIN as a magic number to represent "cancelled".
 *
 * Return true if this changed to cancelled, or false if it was already
 * cancelled.
 */
bool cancel(std::atomic<int64_t>*);

/**
 * Consume (remove) credits from the 'current' atomic<int64_t>.
 *
 * This MUST only be used to remove credits after emitting a value via onNext.
 */
int64_t consume(std::atomic<int64_t>*, int64_t);

/**
 * Whether the current value represents a "cancelled" subscription.
 */
bool isCancelled(std::atomic<int64_t>*);

/**
 * If the requested value is MAX so we can ignore flow control.
 */
bool isInfinite(std::atomic<int64_t>*);

}
}
