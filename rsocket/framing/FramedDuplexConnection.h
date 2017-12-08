// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/DuplexConnection.h"
#include "rsocket/internal/Common.h"

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

  void send(std::unique_ptr<folly::IOBuf>) override;

  void setInput(yarpl::Reference<DuplexConnection::Subscriber>) override;

  bool isFramed() const override {
    return true;
  }

  DuplexConnection* getConnection() {
    return inner_.get();
  }

 private:
  std::unique_ptr<DuplexConnection> inner_;
  yarpl::Reference<FramedReader> inputReader_;
  std::shared_ptr<ProtocolVersion> protocolVersion_;
};
}
