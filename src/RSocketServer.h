// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <mutex>

#include <folly/Baton.h>
#include <folly/Synchronized.h>
#include <folly/ThreadLocal.h>
#include "src/ConnectionAcceptor.h"
#include "src/RSocketParameters.h"
#include "src/RSocketResponder.h"
#include "src/RSocketSetup.h"
#include "src/internal/SetupResumeAcceptor.h"

namespace rsocket {

class RSocketConnectionManager;

using OnRSocketSetup = std::function<void(RSocketSetup&)>;
using OnRSocketResume = std::function<void(ResumeParameters&)>;

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
  void start(OnRSocketSetup);

  /**
   * Start the ConnectionAcceptor and begin handling connections.
   *
   * This method will block the calling thread as long as the server is running.
   * It will throw an exception if a failure occurs on startup.
   *
   * This method assumes it will be called only once.
   */
  void startAndPark(OnRSocketSetup);

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

  // TODO version supporting RSocketStats and other params
  // RSocketServer::start(OnRSocketSetup onRSocketSetup, ServerSetup setupParams)

  void acceptConnection(
      std::unique_ptr<DuplexConnection> connection,
      folly::EventBase & eventBase,
      OnRSocketSetup onRSocketSetup);

  void shutdownAndWait();

  /**
   * Gets the port the ConnectionAcceptor is listening on.  Returns folly::none
   * if this server is not listening on a port.
   */
  folly::Optional<uint16_t> listeningPort() const;

 private:

  void onRSocketSetup(
      OnRSocketSetup onRSocketSetup,
      yarpl::Reference<rsocket::FrameTransport> frameTransport,
      rsocket::SetupParameters setupPayload);
  bool onRSocketResume(
      OnRSocketResume onRSocketResume,
      yarpl::Reference<rsocket::FrameTransport> frameTransport,
      rsocket::ResumeParameters setupPayload);

  std::unique_ptr<ConnectionAcceptor> duplexConnectionAcceptor_;
  bool started{false};

  class SetupResumeAcceptorTag{};
  folly::ThreadLocal<rsocket::SetupResumeAcceptor, SetupResumeAcceptorTag> setupResumeAcceptors_;

  folly::Baton<> waiting_;
  std::atomic<bool> isShutdown_{false};

  std::unique_ptr<RSocketConnectionManager> connectionManager_;
};
} // namespace rsocket
