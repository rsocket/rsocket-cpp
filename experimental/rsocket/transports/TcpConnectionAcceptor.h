// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/AsyncServerSocket.h>
#include "rsocket/ConnectionAcceptor.h"

using namespace folly;
using namespace reactivesocket;

namespace rsocket {

/**
 * TCP implementation of ConnectionAcceptor for use with RSocket::createServer
 *
 * Creation of this does nothing. The 'start' method kicks off work.
 *
 * When started it will create a Thread, EventBase, and AsyncServerSocket.
 *
 * Destruction will shut down the thread(s) and socket.
 */
class TcpConnectionAcceptor : public ConnectionAcceptor,
                              public AsyncServerSocket::AcceptCallback {
 public:
  static std::unique_ptr<ConnectionAcceptor> create(int port);

  explicit TcpConnectionAcceptor(int port);
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
   * This can only be called once.
   *
   * @param onAccept
   */
  void start(std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)>
                 onAccept) override;

 private:
  // TODO this is single-threaded right now
  // TODO need to tell it how many threads to run on
  EventBase eventBase_;
  folly::SocketAddress addr_;
  std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)> onAccept_;
  std::shared_ptr<AsyncServerSocket> serverSocket_;

  virtual void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override;

  virtual void acceptError(const std::exception& ex) noexcept override;
};
}
