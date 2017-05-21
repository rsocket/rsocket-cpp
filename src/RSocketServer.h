// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <mutex>

#include <folly/Baton.h>
#include <folly/Synchronized.h>

#include "src/ConnectionAcceptor.h"
#include "src/RSocketParameters.h"
#include "src/RSocketResponder.h"
#include "src/internal/SetupResumeAcceptor.h"

namespace rsocket {

class RSocketStateMachine;

using OnSetupConnection = std::function<std::shared_ptr<RSocketResponder>(SetupParameters&)>;
using OnResumeConnection = std::function<void(ResumeParameters&)>;

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
  void start(OnSetupConnection);

  /**
   * Start the ConnectionAcceptor and begin handling connections.
   *
   * This method will block the calling thread as long as the server is running.
   * It will throw an exception if a failure occurs on startup.
   *
   * This method assumes it will be called only once.
   */
  void startAndPark(OnSetupConnection);

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
  // RSocketServer::start(OnSetupConnection onSetupConnection, ServerSetup setupParams)

  friend class RSocketServerConnectionHandler;

 private:

  void onSetupConnection(
      OnSetupConnection onSetupConnection,
      std::shared_ptr<rsocket::FrameTransport> frameTransport,
      rsocket::SetupParameters setupPayload);
  void onResumeConnection(
      OnResumeConnection onResumeConnection,
      std::shared_ptr<rsocket::FrameTransport> frameTransport,
      rsocket::ResumeParameters setupPayload);

    void addConnection(
      std::shared_ptr<rsocket::RSocketStateMachine>,
      folly::Executor&);
  void removeConnection(std::shared_ptr<rsocket::RSocketStateMachine>);

  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ConnectionAcceptor> duplexConnectionAcceptor_;
  rsocket::SetupResumeAcceptor setupResumeAcceptor_;
  bool started{false};

  /// Set of currently open ReactiveSockets.
  folly::Synchronized<
      std::unordered_map<
          std::shared_ptr<rsocket::RSocketStateMachine>,
          folly::Executor&>,
      std::mutex>
      sockets_;

  folly::Baton<> waiting_;
  folly::Optional<folly::Baton<>> shutdown_;
};
} // namespace rsocket
