// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/AsyncSocket.h>
#include <src/Stats.h>
#include "src/DuplexConnection.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

class TcpReaderWriter;

class TcpDuplexConnection : public DuplexConnection {
 public:
  explicit TcpDuplexConnection(
      folly::AsyncSocket::UniquePtr&& socket,
      folly::Executor& executor,
      std::shared_ptr<Stats> stats = Stats::noop());
  ~TcpDuplexConnection();

  //
  // both getOutput and setOutput are ok to be called multiple times
  // on a single instance of TcpDuplexConnection
  // the latest input/output will be used
  //

  std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> getOutput()
      override;

  void setInput(std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
                    framesSink) override;

 private:
  std::shared_ptr<TcpReaderWriter> tcpReaderWriter_;
  std::shared_ptr<Stats> stats_;
  folly::Executor& executor_;
};
} // reactivesocket
