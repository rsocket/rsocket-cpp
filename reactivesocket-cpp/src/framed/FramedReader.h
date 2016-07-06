// Copyright 2004-present Facebook.  All rights reserved.
#pragma once

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBufQueue.h>
#include <reactive-streams/utilities/AllowanceSemaphore.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace reactivesocket {

class FramedReader : public reactivesocket::Subscriber<Payload>, public reactivesocket::Subscription {
 public:
  explicit FramedReader(reactivesocket::Subscriber<Payload>& frames)
      : frames_(&frames),
        payloadQueue_(folly::IOBufQueue::cacheChainLength()) {}

  // Subscriber methods
  void onSubscribe(reactivesocket::Subscription& subscription) override;
  void onNext(reactivesocket::Payload element) override;
  void onComplete() override;
  void onError(folly::exception_wrapper ex) override;

  // Subscription methods
  void request(size_t n) override;
  void cancel() override;

 private:
  void parseFrames();
  void requestStream();

  SubscriberPtr<reactivesocket::Subscriber<Payload>> frames_;
  SubscriptionPtr<::reactivestreams::Subscription> streamSubscription_;

  ::reactivestreams::AllowanceSemaphore allowance_{0};

  bool streamRequested_{false};
  bool dispatchingFrames_{false};

  folly::IOBufQueue payloadQueue_;
};

} // reactive socket
