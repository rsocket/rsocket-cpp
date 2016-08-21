// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBufQueue.h>
#include <reactive-streams/utilities/AllowanceSemaphore.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

class FramedReader
    : public reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>,
      public reactivesocket::Subscription {
 public:
  FramedReader(
      reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>& frames)
      : frames_(&frames),
        payloadQueue_(folly::IOBufQueue::cacheChainLength()) {}

  // Subscriber methods
  void onSubscribe(Subscription& subscription) override;
  void onNext(std::unique_ptr<folly::IOBuf> element) override;
  void onComplete() override;
  void onError(folly::exception_wrapper ex) override;

  // Subscription methods
  void request(size_t n) override;
  void cancel() override;

 private:
  void parseFrames();
  void requestStream();

  SubscriberPtr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      frames_;
  SubscriptionPtr<Subscription> streamSubscription_;

  ::reactivestreams::AllowanceSemaphore allowance_{0};

  bool streamRequested_{false};
  bool dispatchingFrames_{false};

  folly::IOBufQueue payloadQueue_;
};

} // reactive socket
