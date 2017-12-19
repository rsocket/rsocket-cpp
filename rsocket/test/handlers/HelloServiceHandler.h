// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/RSocketServiceHandler.h"

namespace rsocket {
namespace tests {

// A minimal RSocketServiceHandler which supports resumption.

class HelloServiceHandler : public RSocketServiceHandler {
 public:
  explicit HelloServiceHandler(
      std::shared_ptr<RSocketConnectionEvents> connEvents = nullptr)
      : connectionEvents_(connEvents) {}

  folly::Expected<RSocketConnectionParams, RSocketException> onNewSetup(
      const SetupParameters&) override;

  void onNewRSocketState(
      std::shared_ptr<RSocketServerState> state,
      ResumeIdentificationToken token) override;

  folly::Expected<std::shared_ptr<RSocketServerState>, RSocketException>
  onResume(ResumeIdentificationToken token) override;

 private:
  std::shared_ptr<RSocketConnectionEvents> connectionEvents_;
  folly::Synchronized<
      std::map<ResumeIdentificationToken, std::shared_ptr<RSocketServerState>>,
      std::mutex>
      store_;
};

} // namespace tests
} // namespace rsocket
