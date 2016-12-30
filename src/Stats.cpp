// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/Stats.h"
#include <src/tcp/TcpDuplexConnection.h>

namespace reactivesocket {

class NoopStats : public Stats {
 public:
  void socketCreated() override{};
  void socketDisconnected() override{};
  void socketClosed() override{};

  void duplexConnectionCreated(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override{};
  void duplexConnectionClosed(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override{};

  void bytesWritten(size_t bytes) override{};
  void bytesRead(size_t bytes) override{};
  void frameWritten(const std::string& frameType) override{};
  void frameRead(const std::string& frameType) override{};

  static NoopStats& instance(void) {
    static NoopStats singleton;
    return singleton;
  }

 protected:
  NoopStats() = default;
  ~NoopStats() = default;

 private:
  NoopStats(const NoopStats& other) = delete; // non construction-copyable
  NoopStats& operator=(const NoopStats&) = delete; // non copyable
  NoopStats& operator=(const NoopStats&&) = delete; // non movable
  NoopStats(NoopStats&&) = delete; // non construction-movable
};

Stats& Stats::noop() {
  return NoopStats::instance();
};
}
