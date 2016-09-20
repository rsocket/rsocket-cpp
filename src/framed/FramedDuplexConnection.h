// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <src/Stats.h>
#include "src/DuplexConnection.h"
#include "src/framed/FramedReader.h"

namespace reactivesocket {

class FramedReader;
class FramedWriter;

class FramedDuplexConnection : public virtual DuplexConnection {
 public:
  explicit FramedDuplexConnection(std::unique_ptr<DuplexConnection> connection);
  ~FramedDuplexConnection();

  Subscriber<std::unique_ptr<folly::IOBuf>>& getOutput() noexcept override;
  void setInput(Subscriber<std::unique_ptr<folly::IOBuf>>& framesSink) override;

 private:
  std::unique_ptr<DuplexConnection> connection_;
  FramedReader::UniquePtr inputReader_;
  std::unique_ptr<FramedWriter> outputWriter_;
};

} // reactive socket
