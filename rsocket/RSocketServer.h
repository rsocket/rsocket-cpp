// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <mutex>

#include <folly/Baton.h>
#include <folly/Synchronized.h>
#include <folly/ThreadLocal.h>
#include "rsocket/RSocketServiceHandler.h"
#include "rsocket/ConnectionAcceptor.h"
#include "rsocket/RSocketParameters.h"
#include "rsocket/RSocketResponder.h"
#include "rsocket/internal/SetupResumeAcceptor.h"

namespace rsocket {

/**
 * API for starting an RSocket server. Returned from RSocket::createServer.
 *
 * This listens for connections using a transport from the provided
 * ConnectionAcceptor.
 *
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
  void start(std::shared_ptr<RSocketServiceHandler> serviceHandler);
  void start(OnNewSetupFn onNewSetupFn);

  /**
   * Start the ConnectionAcceptor and begin handling connections.
   *
   * This method will block the calling thread as long as the server is running.
   * It will throw an exception if a failure occurs on startup.
   *
   * The provided RSocketServiceHandler will be used to handle all connections
   * to this server. If you wish to use different RSocketServiceHandler for
   * each connection, then refer to acceptConnection()
   *
   * This method assumes it will be called only once.
   */
  void startAndPark(std::shared_ptr<RSocketServiceHandler> serviceHandler);
  void startAndPark(OnNewSetupFn onNewSetupFn);

  /**
   * Unblock the server if it has called startAndPark().  Can only be called
   * once.
   */
  void unpark();

  /**
   * Accept RSocket connection over the provided DuplexConnection.  The
   * provided RSocketServiceHandler will be used to handle the connection.
   */
  void acceptConnection(
      std::unique_ptr<DuplexConnection> connection,
      folly::EventBase& eventBase,
      std::shared_ptr<RSocketServiceHandler> serviceHandler);

  void shutdownAndWait();

  /**
   * Gets the port the ConnectionAcceptor is listening on.  Returns folly::none
   * if this server is not listening on a port.
   */
  folly::Optional<uint16_t> listeningPort() const;

 private:
  void onRSocketSetup(
      std::shared_ptr<RSocketServiceHandler> serviceHandler,
      yarpl::Reference<rsocket::FrameTransport> frameTransport,
      rsocket::SetupParameters setupPayload);
  void onRSocketResume(
      std::shared_ptr<RSocketServiceHandler> serviceHandler,
      yarpl::Reference<rsocket::FrameTransport> frameTransport,
      rsocket::ResumeParameters setupPayload);

  std::unique_ptr<ConnectionAcceptor> duplexConnectionAcceptor_;
  bool started{false};

  class SetupResumeAcceptorTag{};
  folly::ThreadLocal<rsocket::SetupResumeAcceptor, SetupResumeAcceptorTag> setupResumeAcceptors_;

  folly::Baton<> waiting_;
  std::atomic<bool> isShutdown_{false};

  std::shared_ptr<ConnectionSet> connectionSet_;
};
} // namespace rsocket
