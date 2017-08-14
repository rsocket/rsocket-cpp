// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <vector>

#include "rsocket/DuplexConnection.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {

struct ProtocolVersion;

class FramedWriter : public DuplexConnection::Subscriber {
 public:
  explicit FramedWriter(
      yarpl::Reference<DuplexConnection::Subscriber> stream,
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

  yarpl::Reference<DuplexConnection::Subscriber> stream_;
  std::shared_ptr<ProtocolVersion> protocolVersion_;
};
}
