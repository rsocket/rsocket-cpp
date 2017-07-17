// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <gmock/gmock.h>
#include "rsocket/RSocketStats.h>
#include "rsocket/transports/tcp/TcpDuplexConnection.h>

#include "rsocket/Payload.h"

namespace rsocket {

class MockStats : public RSocketStats {
 public:
  MOCK_METHOD0(socketCreated, void());
  MOCK_METHOD1(socketClosed, void(StreamCompletionSignal));
  MOCK_METHOD0(socketDisconnected, void());

  MOCK_METHOD2(
      duplexConnectionCreated,
      void(const std::string&, rsocket::DuplexConnection*));
  MOCK_METHOD2(
      duplexConnectionClosed,
      void(const std::string&, rsocket::DuplexConnection*));

  MOCK_METHOD1(bytesWritten, void(size_t));
  MOCK_METHOD1(bytesRead, void(size_t));
  MOCK_METHOD1(frameWritten, void(FrameType));
  MOCK_METHOD1(frameRead, void(FrameType));
  MOCK_METHOD2(resumeBufferChanged, void(int, int));
  MOCK_METHOD2(streamBufferChanged, void(int64_t, int64_t));
};
}
