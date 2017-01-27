// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <gmock/gmock.h>
#include <src/Stats.h>
#include <src/tcp/TcpDuplexConnection.h>

#include "src/Payload.h"

namespace reactivesocket {

class MockStats : public Stats {
 public:
  MOCK_METHOD0(socketCreated, void());
  MOCK_METHOD1(socketClosed, void(StreamCompletionSignal));
  MOCK_METHOD0(socketDisconnected, void());

  MOCK_METHOD2(
      duplexConnectionCreated,
      void(const std::string&, reactivesocket::DuplexConnection*));
  MOCK_METHOD2(
      duplexConnectionClosed,
      void(const std::string&, reactivesocket::DuplexConnection*));

  MOCK_METHOD1(bytesWritten, void(size_t));
  MOCK_METHOD1(bytesRead, void(size_t));
  MOCK_METHOD1(frameWritten, void(const std::string&));
  MOCK_METHOD1(frameRead, void(const std::string&));
  MOCK_METHOD2(resumeBufferChanged, void(int, int));
};
}
