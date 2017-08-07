// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/AsyncSocket.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/RSocketStats.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {

class TcpReaderWriter;

class TcpDuplexConnection : public DuplexConnection {
 public:
  explicit TcpDuplexConnection(
      folly::AsyncSocket::UniquePtr&& socket,
      std::shared_ptr<RSocketStats> stats = RSocketStats::noop());
  ~TcpDuplexConnection();

  yarpl::Reference<yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>>
  getOutput() override;

  void setInput(
      yarpl::Reference<
          yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>>) override;

 private:
  std::shared_ptr<TcpReaderWriter> tcpReaderWriter_;
  std::shared_ptr<RSocketStats> stats_;
};
} // namespace rsocket
