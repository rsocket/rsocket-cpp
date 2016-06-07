// Copyright 2004-present Facebook.  All rights reserved.
#pragma once

#include "reactivesocket-cpp/src/DuplexConnection.h"

namespace reactivesocket {

class FramedReader;
class FramedWriter;

class FramedDuplexConnection : public DuplexConnection {
 public:
  explicit FramedDuplexConnection(std::unique_ptr<DuplexConnection> connection);
  ~FramedDuplexConnection();

  Subscriber<Payload>& getOutput() noexcept override;
  void setInput(Subscriber<Payload>& framesSink) override;

 private:
  std::unique_ptr<DuplexConnection> connection_;
  std::unique_ptr<FramedReader> inputReader_;
  std::unique_ptr<FramedWriter> outputWriter_;
};

} // reactive socket
