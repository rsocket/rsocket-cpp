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
      Stats& stats = Stats::noop());
  ~TcpDuplexConnection();

  std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> getOutput()
      override;

  void setInput(std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
                    framesSink) override;

 private:
  std::shared_ptr<TcpReaderWriter> tcpReaderWriter_;
  Stats& stats_;
};
} // reactivesocket
