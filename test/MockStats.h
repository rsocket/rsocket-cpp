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
  MOCK_METHOD0(socketCreated_, void());
  MOCK_METHOD0(socketClosed_, void());
  MOCK_METHOD2(
      connectionCreated_,
      void(const std::string&, reactivesocket::DuplexConnection*));
  MOCK_METHOD2(
      connectionClosed_,
      void(const std::string&, reactivesocket::DuplexConnection*));
  MOCK_METHOD1(bytesWritten_, void(size_t));
  MOCK_METHOD1(bytesRead_, void(size_t));
  MOCK_METHOD1(frameWritten_, void(const std::string&));
  MOCK_METHOD1(frameRead_, void(const std::string&));

  void socketCreated() override {
    socketCreated_();
  }

  void socketClosed() override {
    socketClosed_();
  }

  void connectionCreated(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override {
    connectionCreated_(type, connection);
  }

  void connectionClosed(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override {
    connectionClosed_(type, connection);
  }

  virtual void bytesWritten(size_t bytes) override {
    bytesWritten_(bytes);
  }

  virtual void bytesRead(size_t bytes) override {
    bytesRead_(bytes);
  }

  virtual void frameWritten(const std::string& frameType) override {
    frameWritten_(frameType);
  }

  virtual void frameRead(const std::string& frameType) override {
    frameRead_(frameType);
  }
};
}
