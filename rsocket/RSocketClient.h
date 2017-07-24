// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/futures/Future.h>

#include "rsocket/ColdResumeHandler.h"
#include "rsocket/ConnectionFactory.h"
#include "rsocket/RSocketNetworkStats.h"
#include "rsocket/RSocketParameters.h"
#include "rsocket/RSocketRequester.h"
#include "rsocket/RSocketStats.h"
#include "rsocket/ResumeManager.h"

namespace rsocket {

class RSocket;
class RSocketConnectionManager;

/**
 * API for connecting to an RSocket server. Created with RSocket class.
 * This connects using a transport from the provided ConnectionFactory.
 */
class RSocketClient {
 public:
  ~RSocketClient();

  RSocketClient(const RSocketClient&) = delete; // copy
  RSocketClient(RSocketClient&&) = default; // move
  RSocketClient& operator=(const RSocketClient&) = delete; // copy
  RSocketClient& operator=(RSocketClient&&) = default; // move

  friend class RSocket;

  // Returns the RSocketRequester associated with the RSocketClient.
  std::shared_ptr<RSocketRequester> getRequester() const;

  // Resumes the connection.  If a stateMachine already exists,
  // it provides a warm-resumption.  If a stateMachine does not exist,
  // it does a cold-resumption.  The returned future resolves on successful
  // resumption.  Else either a ConnectionException or a ResumptionException
  // is raised.
  folly::Future<folly::Unit> resume();

 private:
  // Private constructor.  RSocket class should be used to create instances
  // of RSocketClient.
  RSocketClient(
      std::unique_ptr<ConnectionFactory>,
      SetupParameters setupParameters = SetupParameters(),
      std::shared_ptr<RSocketResponder> responder =
          std::make_shared<RSocketResponder>(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer =
          std::unique_ptr<KeepaliveTimer>(),
      std::shared_ptr<RSocketStats> stats = RSocketStats::noop(),
      std::shared_ptr<RSocketNetworkStats> networkStats =
          std::shared_ptr<RSocketNetworkStats>(),
      std::shared_ptr<ResumeManager> resumeManager =
          std::shared_ptr<ResumeManager>(),
      std::shared_ptr<ColdResumeHandler> coldResumeHandler =
          std::shared_ptr<ColdResumeHandler>(),
      OnRSocketResume onRSocketResume =
          [](std::vector<StreamId>, std::vector<StreamId>) { return false; });

  // Connects to the remote side and creates state.
  folly::Future<folly::Unit> connect();

  // Creates RSocketStateMachine and RSocketRequester
  void createState(folly::EventBase& eventBase);

  std::unique_ptr<ConnectionFactory> connectionFactory_;
  std::unique_ptr<RSocketConnectionManager> connectionManager_;
  SetupParameters setupParameters_;
  std::shared_ptr<RSocketResponder> responder_;
  std::unique_ptr<KeepaliveTimer> keepaliveTimer_;
  std::shared_ptr<RSocketStats> stats_;
  std::shared_ptr<RSocketNetworkStats> networkStats_;
  std::shared_ptr<ResumeManager> resumeManager_;
  std::shared_ptr<ColdResumeHandler> coldResumeHandler_;

  std::shared_ptr<RSocketStateMachine> stateMachine_;
  std::shared_ptr<RSocketRequester> requester_;

  // Remember the evb on which the client was created.  Ensure warme-resume()
  // operations are done on the same evb.
  folly::EventBase* evb_;
};
}
