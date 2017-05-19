// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "RSocketResponder.h"
#include "src/ConnectionResumeRequest.h"
#include "src/ConnectionSetupRequest.h"
#include "src/framing/FrameTransport.h"
#include "src/temporary_home/ServerConnectionAcceptor.h"

namespace rsocket {

class RSocketStateMachine;

/**
 * Handles the setup/creation/error steps of an RSocket. This is an abstract
 * class that is responsible for basic RSocket creation and setup; the virtual
 * functions will be implemented to customize the actual handling of the
 * RSocket.
 * Handles the setup/creation/error steps for an RSocketServer accepting
 * connections.
 *
 * This is an abstract class that is responsible for basic RSocket creation and
 * setup.
 *
 * The virtual functions will be implemented to customize the actual handling of
 * the RSocket.
 *
 * TODO: Resumability
 */
class RSocketConnectionHandler : public rsocket::ConnectionHandler {
 public:
  virtual void setupNewSocket(
      std::shared_ptr<rsocket::FrameTransport> frameTransport,
      rsocket::SetupParameters setupPayload) override;

  virtual bool resumeSocket(
      std::shared_ptr<rsocket::FrameTransport> frameTransport,
      rsocket::ResumeParameters) override;

  virtual void connectionError(
      std::shared_ptr<rsocket::FrameTransport>,
      folly::exception_wrapper ex) override;

 private:
  /**
   * An RSocketResponder is responsible for translating a request stream
   * into action. This function provides the appropriate request handler for
   * an RSocket given the setup of the socket.
   */
  virtual std::shared_ptr<RSocketResponder> getRequestResponder(
      std::shared_ptr<ConnectionSetupRequest> request) = 0;

  /**
   * Different connection handlers can customize the way that they manage and
   * store RSocket connections.
   */
  virtual void manageSocket(
      std::shared_ptr<ConnectionSetupRequest> request,
      std::shared_ptr<rsocket::RSocketStateMachine> socket) = 0;
};

} // namespace rsocket
