// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/RSocketParameters.h"
#include "yarpl/Refcounted.h"

namespace folly {
class EventBase;
}

namespace rsocket {

class FrameTransport;
class RSocketConnectionManager;
class RSocketError;
class RSocketNetworkStats;
class RSocketRequester;
class RSocketResponder;
class RSocketStateMachine;
class RSocketStats;

class RSocketSetup {
 public:
  RSocketSetup(
      yarpl::Reference<FrameTransport> frameTransport,
      SetupParameters setupParams,
      folly::EventBase& eventBase,
      RSocketConnectionManager& connectionManager);
  ~RSocketSetup();

  SetupParameters& params() {
    return setupParams_;
  }

  void createRSocket(
      std::shared_ptr<RSocketResponder> requestResponder,
      std::shared_ptr<RSocketStats> stats = std::shared_ptr<RSocketStats>(),
      std::shared_ptr<RSocketNetworkStats> networkStats = std::shared_ptr<RSocketNetworkStats>());

  std::unique_ptr<RSocketRequester> createRSocketRequester(
      std::shared_ptr<RSocketResponder> requestResponder,
      std::shared_ptr<RSocketStats> stats = std::shared_ptr<RSocketStats>(),
      std::shared_ptr<RSocketNetworkStats> networkStats = std::shared_ptr<RSocketNetworkStats>());

  void error(const RSocketError&);

 private:

  std::shared_ptr<RSocketStateMachine> createRSocketStateMachine(
      std::shared_ptr<RSocketResponder> requestResponder,
      std::shared_ptr<RSocketStats> stats,
      std::shared_ptr<RSocketNetworkStats> networkStats);

  yarpl::Reference<FrameTransport> frameTransport_;
  SetupParameters setupParams_;
  folly::EventBase& eventBase_;
  RSocketConnectionManager& connectionManager_;
};
}
