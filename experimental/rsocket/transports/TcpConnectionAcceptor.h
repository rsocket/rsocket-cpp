// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/AsyncServerSocket.h>
#include "rsocket/ConnectionAcceptor.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

class TcpConnectionAcceptor : public ConnectionAcceptor,
                              public AsyncServerSocket::AcceptCallback {
 public:
  static std::unique_ptr<ConnectionAcceptor> create(int port);

  TcpConnectionAcceptor(int port);
  virtual ~TcpConnectionAcceptor();
  TcpConnectionAcceptor(const TcpConnectionAcceptor&) = delete; // copy
  TcpConnectionAcceptor(TcpConnectionAcceptor&&) = delete; // move
  TcpConnectionAcceptor& operator=(const TcpConnectionAcceptor&) =
      delete; // copy
  TcpConnectionAcceptor& operator=(TcpConnectionAcceptor&&) = delete; // move

  /**
   * Create an EventBase, Thread, and AsyncServerSocket. Bind to the given port
   * and start accepting TCP connections.
   *
   * @param onAccept
   */
  void start(std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)>
                 onAccept) override;

 private:
  // TODO this is single-threaded right now
  // TODO need to tell it how many threads to run on
  EventBase eventBase;
  std::thread thread;
  folly::SocketAddress addr;
  std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)> onAccept;
  std::shared_ptr<AsyncServerSocket> serverSocket;

  virtual void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override;

  virtual void acceptError(const std::exception& ex) noexcept override;
};
}
