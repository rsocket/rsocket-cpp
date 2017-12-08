// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/AsyncTransport.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/RSocketStats.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {

class TcpReaderWriter;

class TcpDuplexConnection : public DuplexConnection {
 public:
  explicit TcpDuplexConnection(
      folly::AsyncTransportWrapper::UniquePtr&& socket,
      std::shared_ptr<RSocketStats> stats = RSocketStats::noop());
  ~TcpDuplexConnection();

  void send(std::unique_ptr<folly::IOBuf>) override;

  void setInput(yarpl::Reference<DuplexConnection::Subscriber>) override;

  // Only to be used for observation purposes.
  folly::AsyncTransportWrapper* getTransport();

 private:
  boost::intrusive_ptr<TcpReaderWriter> tcpReaderWriter_;
  std::shared_ptr<RSocketStats> stats_;
};
}
