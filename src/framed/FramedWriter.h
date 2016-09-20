// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include <vector>
#include "src/ReactiveStreamsCompat.h"

namespace folly {
class IOBuf;
}

namespace reactivesocket {

class FramedWriter
    : public reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>,
      public reactivesocket::Subscription {
 public:
  explicit FramedWriter(
      reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>& stream)
      : stream_(&stream) {}

  // Subscriber methods
  void onSubscribe(reactivesocket::Subscription& subscription) override;
  void onNext(std::unique_ptr<folly::IOBuf> element) override;
  void onComplete() override;
  void onError(folly::exception_wrapper ex) override;

  // Subscription methods
  void request(size_t n) override;
  void cancel() override;

  void onNextMultiple(std::vector<std::unique_ptr<folly::IOBuf>> element);

 private:
  SubscriberPtr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      stream_;
  SubscriptionPtr<::reactivestreams::Subscription> writerSubscription_;
};

} // reactive socket
