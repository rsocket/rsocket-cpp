// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/AsyncServerSocket.h>
#include "ServerConnectionAcceptor.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

class TcpServerConnectionAcceptor : public ServerConnectionAcceptor,
                                    public AsyncServerSocket::AcceptCallback {
 public:
  static std::unique_ptr<ServerConnectionAcceptor> create(int port);

  TcpServerConnectionAcceptor(int port) {
    addr.setFromLocalPort(port);
  }

  ~TcpServerConnectionAcceptor();

  /**
   * Create an EventBase, Thread, and AsyncServerSocket. Bind to the given port
   * and start accepting TCP connections.
   *
   * @param onAccept
   */
  void start(OnAccept onAccept) override;

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
