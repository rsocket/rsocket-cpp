// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/DuplexConnection.h"
#include "src/internal/Common.h"

namespace rsocket {

class FramedReader;
class FramedWriter;
struct ProtocolVersion;

class FramedDuplexConnection : public virtual DuplexConnection {
 public:
  FramedDuplexConnection(
      std::unique_ptr<DuplexConnection> connection,
      ProtocolVersion protocolVersion);

  ~FramedDuplexConnection();

  yarpl::Reference<yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>>
  getOutput() noexcept override;

  void setInput(yarpl::Reference<yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>>
                    framesSink) override;

  bool isFramed() override {
    return true;
  }

 private:
  std::unique_ptr<DuplexConnection> inner_;
  yarpl::Reference<FramedReader> inputReader_;
  std::shared_ptr<ProtocolVersion> protocolVersion_;
};

} // reactivesocket
