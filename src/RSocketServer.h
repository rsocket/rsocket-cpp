// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <mutex>

#include <folly/Baton.h>
#include <folly/Synchronized.h>

#include "src/ConnectionAcceptor.h"
#include "src/ConnectionSetupRequest.h"
#include "src/RSocketResponder.h"
#include "src/temporary_home/ServerConnectionAcceptor.h"

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
   * This method blocks until the server has started. It returns if successful
   * or throws an exception if failure occurs.
   *
   * This method assumes it will be called only once.
   */
  void start(OnAccept);

  /**
   * Start the ConnectionAcceptor and begin handling connections.
   *
   * This method will block the calling thread as long as the server is running.
   * It will throw an exception if a failure occurs on startup.
   *
   * This method assumes it will be called only once.
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
  void addConnection(std::shared_ptr<reactivesocket::RSocketStateMachine>, folly::Executor&);
  void removeConnection(std::shared_ptr<reactivesocket::RSocketStateMachine>);

  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ConnectionAcceptor> lazyAcceptor_;
  reactivesocket::ServerConnectionAcceptor acceptor_;
  bool started{false};

  /// Set of currently open ReactiveSockets.
  folly::Synchronized<
      std::unordered_map<std::shared_ptr<reactivesocket::RSocketStateMachine>, folly::Executor&>,
      std::mutex>
      sockets_;

  folly::Baton<> waiting_;
  folly::Optional<folly::Baton<>> shutdown_;
};
} // namespace rsocket
