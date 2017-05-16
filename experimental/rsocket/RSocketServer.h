// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <mutex>

#include <folly/Baton.h>
#include <folly/Synchronized.h>

#include "rsocket/ConnectionAcceptor.h"
#include "rsocket/ConnectionSetupRequest.h"
#include "rsocket/RSocketResponder.h"
#include "src/ReactiveSocket.h"
#include "src/ServerConnectionAcceptor.h"

namespace rsocket {

using OnAccept = std::function<std::shared_ptr<RSocketResponder>(
    std::shared_ptr<ConnectionSetupRequest>)>;
/**
 * API for starting an RSocket server. Returned from RSocket::createServer.
 *
 * This listens for connections using a transport from the provided
 * ConnectionAcceptor.
 *
 * TODO: Resumability
 *
 * TODO: Concurrency (number of threads)
 */
class RSocketServer {
 public:
  explicit RSocketServer(std::unique_ptr<ConnectionAcceptor>);
  ~RSocketServer();

  RSocketServer(const RSocketServer&) = delete;
  RSocketServer(RSocketServer&&) = delete;

  RSocketServer& operator=(const RSocketServer&) = delete;
  RSocketServer& operator=(RSocketServer&&) = delete;

  /**
   * Start the ConnectionAcceptor and begin handling connections.
   *
   * This method is asynchronous.
   */
  void start(OnAccept);

  /**
   * Start the ConnectionAcceptor and begin handling connections.
   *
   * This method will block the calling thread.
   */
  void startAndPark(OnAccept);

  /**
   * Unblock the server if it has called startAndPark().  Can only be called
   * once.
   */
  void unpark();

  // TODO version supporting RESUME
  //  void start(
  //      std::function<std::shared_ptr<RequestHandler>(
  //          std::unique_ptr<ConnectionSetupRequest>)>,
  //      // TODO what should a ResumeRequest return?
  //      std::function<std::shared_ptr<RequestHandler>(
  //          std::unique_ptr<ConnectionResumeRequest>)>);

  // TODO version supporting Stats and other params
  // RSocketServer::start(OnAccept onAccept, ServerSetup setupParams)

  friend class RSocketServerConnectionHandler;

 private:
  void addSocket(std::unique_ptr<reactivesocket::ReactiveSocket>);
  void removeSocket(reactivesocket::ReactiveSocket*);

  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ConnectionAcceptor> lazyAcceptor_;
  reactivesocket::ServerConnectionAcceptor acceptor_;
  std::shared_ptr<reactivesocket::ConnectionHandler> connectionHandler_;

  /// Set of currently open ReactiveSockets.
  folly::Synchronized<
      std::unordered_set<std::unique_ptr<reactivesocket::ReactiveSocket>>,
      std::mutex>
      sockets_;

  folly::Baton<> waiting_;
  folly::Optional<folly::Baton<>> shutdown_;
};
} // namespace rsocket
