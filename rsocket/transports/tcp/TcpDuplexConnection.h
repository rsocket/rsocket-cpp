// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <boost/smart_ptr/intrusive_ptr.hpp>
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

  yarpl::Reference<DuplexConnection::Subscriber> getOutput() override;

  void setInput(yarpl::Reference<DuplexConnection::Subscriber>) override;

 private:
  boost::intrusive_ptr<TcpReaderWriter> tcpReaderWriter_;
  std::shared_ptr<RSocketStats> stats_;
};
}
