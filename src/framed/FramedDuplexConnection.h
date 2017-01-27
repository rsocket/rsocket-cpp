// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <src/Executor.h>
#include <src/Stats.h>
#include "src/DuplexConnection.h"

namespace reactivesocket {

class FramedReader;
class FramedWriter;

class FramedDuplexConnection : public virtual DuplexConnection {
 public:
  explicit FramedDuplexConnection(
      std::unique_ptr<DuplexConnection> connection,
      folly::Executor& executor);
  ~FramedDuplexConnection();

  std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
  getOutput() noexcept override;
  void setInput(std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
                    framesSink) override;

 private:
  std::unique_ptr<DuplexConnection> connection_;
  std::shared_ptr<FramedReader> inputReader_;
  std::shared_ptr<FramedWriter> outputWriter_;
  folly::Executor& executor_;
};

} // reactivesocket
