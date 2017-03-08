// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <condition_variable>
#include <mutex>
#include "rsocket/ConnectionAcceptor.h"
#include "rsocket/ConnectionResumeRequest.h"
#include "rsocket/ConnectionSetupRequest.h"
#include "src/RequestHandler.h"
#include "src/ServerConnectionAcceptor.h"
#include "src/StandardReactiveSocket.h"

namespace rsocket {

using OnSetupNewSocket = std::function<void(
    std::shared_ptr<reactivesocket::FrameTransport> frameTransport,
    reactivesocket::ConnectionSetupPayload setupPayload,
    folly::Executor&)>;

using OnAccept = std::function<std::shared_ptr<reactivesocket::RequestHandler>(
    std::unique_ptr<ConnectionSetupRequest>)>;
/**
 * API for starting an RSocket server. Returned from RSocket::createServer.
 *
 * This listens for connections using a transport from the provided
 * ConnectionAcceptor.
 */
class RSocketServer {
  // TODO resumability
  // TODO concurrency (number of threads)

 public:
  explicit RSocketServer(std::unique_ptr<ConnectionAcceptor>);
  RSocketServer(const RSocketServer&) = delete; // copy
  RSocketServer(RSocketServer&&) = delete; // move
  RSocketServer& operator=(const RSocketServer&) = delete; // copy
  RSocketServer& operator=(RSocketServer&&) = delete; // move

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

  // TODO version supporting RESUME
  //  void start(
  //      std::function<std::shared_ptr<RequestHandler>(
  //          std::unique_ptr<ConnectionSetupRequest>)>,
  //      // TODO what should a ResumeRequest return?
  //      std::function<std::shared_ptr<RequestHandler>(
  //          std::unique_ptr<ConnectionResumeRequest>)>);

  // TODO version supporting Stats and other params
  // RSocketServer::start(OnAccept onAccept, ServerSetup setupParams)

 private:
  std::unique_ptr<ConnectionAcceptor> lazyAcceptor_;
  std::unique_ptr<reactivesocket::ServerConnectionAcceptor> acceptor_;
  std::vector<std::unique_ptr<reactivesocket::ReactiveSocket>> reactiveSockets_;
  std::mutex m_;
  std::condition_variable cv_;

  void removeSocket(reactivesocket::ReactiveSocket& socket);
  void addSocket(std::unique_ptr<reactivesocket::ReactiveSocket> socket);
};
}
