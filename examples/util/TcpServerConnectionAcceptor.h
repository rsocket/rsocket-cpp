// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <string>
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/StandardReactiveSocket.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {
using OnAccept =
    std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)>;

class ServerConnectionAcceptor : public AsyncServerSocket::AcceptCallback {
 public:
  static std::unique_ptr<ServerConnectionAcceptor> tcpServer(int port);

  ServerConnectionAcceptor(int port) {
    addr.setFromLocalPort(port);
  }

  ~ServerConnectionAcceptor();

  /**
   * Create an EventBase, Thread, and AsyncServerSocket. Bind to the given port
   * and start accepting TCP connections.
   *
   * @param onAccept
   */
  void start(OnAccept onAccept);

 private:
  // TODO this is single-threaded right now
  // TODO need to tell it how many threads to run on
  EventBase eventBase;
  std::thread thread;
  folly::SocketAddress addr;
  OnAccept onAccept;
  std::shared_ptr<AsyncServerSocket> serverSocket;

  virtual void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override;

  virtual void acceptError(const std::exception& ex) noexcept override;
};
}
