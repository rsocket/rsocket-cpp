// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/AsyncTransport.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/RSocketStats.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {

class SimpleTcpReaderWriter;
class BatchingTcpReaderWriter;

template <typename ReaderWriter>
class GenericTcpDuplexConnection : public DuplexConnection {
 public:
  explicit GenericTcpDuplexConnection(
      folly::AsyncTransportWrapper::UniquePtr&& socket,
      std::shared_ptr<RSocketStats> stats = RSocketStats::noop());
  ~GenericTcpDuplexConnection();

  yarpl::Reference<DuplexConnection::Subscriber> getOutput() override;

  void setInput(yarpl::Reference<DuplexConnection::Subscriber>) override;

  // Only to be used for observation purposes.
  folly::AsyncTransportWrapper* getTransport();

 private:
  boost::intrusive_ptr<ReaderWriter> tcpReaderWriter_;
  std::shared_ptr<RSocketStats> stats_;
};

// templated GenericTcpDuplexConnection types are instantiated in
// TcpDuplexConnection.cpp

// define TcpDuplexConnection as such to maintain backwards compatibility
using BatchingTcpDuplexConnection =
    GenericTcpDuplexConnection<BatchingTcpReaderWriter>;
using TcpDuplexConnection = GenericTcpDuplexConnection<SimpleTcpReaderWriter>;
}
