// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/futures/Future.h>

#include "rsocket/ColdResumeHandler.h"
#include "rsocket/ConnectionFactory.h"
#include "rsocket/DuplexConnection.h"
#include "rsocket/RSocketConnectionEvents.h"
#include "rsocket/RSocketParameters.h"
#include "rsocket/RSocketRequester.h"
#include "rsocket/RSocketResponder.h"
#include "rsocket/RSocketStats.h"
#include "rsocket/ResumeManager.h"

namespace rsocket {

class RSocket;

/**
 * API for connecting to an RSocket server. Created with RSocket class.
 * This connects using a transport from the provided ConnectionFactory.
 */
class RSocketClient {
 public:
  ~RSocketClient();

  RSocketClient(const RSocketClient&) = delete;
  RSocketClient(RSocketClient&&) = default;
  RSocketClient& operator=(const RSocketClient&) = delete;
  RSocketClient& operator=(RSocketClient&&) = default;

  friend class RSocket;

  // Returns the RSocketRequester associated with the RSocketClient.
  const std::shared_ptr<RSocketRequester>& getRequester() const;

  // Resumes the connection.  If a stateMachine already exists,
  // it provides a warm-resumption.  If a stateMachine does not exist,
  // it does a cold-resumption.  The returned future resolves on successful
  // resumption.  Else either a ConnectionException or a ResumptionException
  // is raised.
  folly::Future<folly::Unit> resume();

  // Disconnect the underlying transport.
  folly::Future<folly::Unit> disconnect(folly::exception_wrapper = {});

 private:
  // Private constructor.  RSocket class should be used to create instances
  // of RSocketClient.
  RSocketClient(
      std::shared_ptr<ConnectionFactory>,
      ProtocolVersion protocolVersion,
      ResumeIdentificationToken token,
      std::shared_ptr<RSocketResponder> responder,
      std::chrono::milliseconds keepaliveInterval,
      std::shared_ptr<RSocketStats> stats,
      std::shared_ptr<RSocketConnectionEvents> connectionEvents,
      std::shared_ptr<ResumeManager> resumeManager,
      std::shared_ptr<ColdResumeHandler> coldResumeHandler,
      folly::EventBase* stateMachineEvb);

  // Create stateMachine with the given DuplexConnection
  void fromConnection(
      std::unique_ptr<DuplexConnection> connection,
      folly::EventBase& transportEvb,
      SetupParameters setupParameters);

  // Creates RSocketStateMachine and RSocketRequester
  void createState();

  std::shared_ptr<ConnectionFactory> connectionFactory_;
  std::shared_ptr<RSocketResponder> responder_;
  std::chrono::milliseconds keepaliveInterval_;
  std::shared_ptr<RSocketStats> stats_;
  std::shared_ptr<RSocketConnectionEvents> connectionEvents_;
  std::shared_ptr<ResumeManager> resumeManager_;
  std::shared_ptr<ColdResumeHandler> coldResumeHandler_;

  std::shared_ptr<RSocketStateMachine> stateMachine_;
  std::shared_ptr<RSocketRequester> requester_;

  ProtocolVersion protocolVersion_;
  ResumeIdentificationToken token_;

  // Remember the StateMachine's evb (supplied through constructor).  If no
  // EventBase is provided, the underlying transport's EventBase will be used
  // to drive the StateMachine.
  // If an EventBase is provided for StateMachine and underlying Transport's
  // EventBase is different from it, then we use Scheduled* classes to let the
  // StateMachine and Transport live on different EventBases.
  // It might happen that the StateMachine and Transport live on same
  // EventBase, but the transport ends up being in different EventBase after
  // resumption, and vice versa.
  folly::EventBase* evb_{nullptr};

};
}
