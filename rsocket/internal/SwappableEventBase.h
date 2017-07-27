// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>
#include <folly/Function.h>
#include <mutex>

namespace rsocket {

// SwappableEventBase provides an interface similar to EventBase, allowing
// an underlying EventBase to be changed, and to force callbacks to be
// executed in serial order regardless of which underlying EventBase they are
// enqueued on.
class SwappableEventBase final {
  // std::mutex doesn't like being in a std::pair
  struct MutexBoolPair {
    // lock for synchronization on destroyed_, and all members of the parent SEB
    std::mutex l_;
    // has the SEB's destructor ran?
    bool destroyed_ {false};
  };

public:
  using CbFunc = folly::Function<void(folly::EventBase&)>;

  explicit SwappableEventBase(folly::EventBase& eb)
  : eb_(&eb),
    nextEb_(nullptr),
    hasSebDtored_(std::make_shared<MutexBoolPair>()) {}

  // Run or enqueue 'cb', in order with all prior calls to runInEventBaseThread
  // If setEventBase has been called, and the prior EventBase is still
  // processing tasks, runInEventBaseThread will queue tasks until the old EB's
  // tasks have all completed. After that, SwappableEventBase will enqueue
  // buffered tasks on the last EB set via setEventBase.
  //
  // Callbacks take a single parameter: the underlying EventBase
  // that the callback is executing on.
  bool runInEventBaseThread(CbFunc cb);

  // Sets the EventBase to enqueue callbacks on, once the current EventBase has
  // drained
  void setEventBase(folly::EventBase& newEb);

  // SwappableEventBase will enqueue tasks on the old eventbase if
  // there are any pending by the time the SEB is destroyed
  ~SwappableEventBase();

private:
  folly::EventBase* eb_;
  folly::EventBase* nextEb_; // also indicate if we're in the middle of a swap

  // is the SwappableEventBase waiting for the current EventBase to finish
  // draining?
  bool isSwapping() const;

  // shared data between the SwappableEventBase and anyone else holding
  // a reference to the SEB (eg, swapping lambda in setEventBase) to avoid
  // accessing a dangling pointer in the case where the SEB has already
  // had its destructor run
  mutable std::shared_ptr<MutexBoolPair> hasSebDtored_;

  // tasks enqueued with runInEventBaseThread while the SEB is waiting for
  // the old EventBase* eb_ to drain
  std::vector<CbFunc> queued_;
};


} /* ns rsocket */
