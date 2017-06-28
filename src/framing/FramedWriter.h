// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <vector>
#include <memory>
#include "yarpl/flowable/Subscriber.h"

namespace folly {
class IOBuf;
}

namespace rsocket {

struct ProtocolVersion;

class FramedWriter : public yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>> {
 using SubscriberBase = yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>;

 public:
  explicit FramedWriter(
      yarpl::Reference<SubscriberBase> stream,
      std::shared_ptr<ProtocolVersion> protocolVersion)
      : stream_(std::move(stream)),
        protocolVersion_(std::move(protocolVersion)) {}

  void onNextMultiple(std::vector<std::unique_ptr<folly::IOBuf>> element);

 private:
  // Subscriber methods
  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) override;
  void onNext(std::unique_ptr<folly::IOBuf> element) override;
  void onComplete() override;
  void onError(std::exception_ptr ex) override;

  void error(std::string errorMsg);

  size_t getFrameSizeFieldLength() const;
  size_t getPayloadLength(size_t payloadLength) const;

  std::unique_ptr<folly::IOBuf> appendSize(
      std::unique_ptr<folly::IOBuf> payload);

  yarpl::Reference<SubscriberBase> stream_;
  std::shared_ptr<ProtocolVersion> protocolVersion_;
};

} // reactivesocket
