#pragma once

#include <string>
#include <thread>

#include <folly/futures/ScheduledExecutor.h>
#include <folly/io/async/EventBase.h>

namespace reactivesocket {

class EventBaseExecutor final : public folly::ScheduledExecutor {
 public:
  explicit EventBaseExecutor(std::string name);

  void startThread();

  void add(folly::Func) override;
  void scheduleAt(folly::Func&&, const TimePoint&) override;

  void prepareStopThread();
  void stopThread();

  ~EventBaseExecutor();

  const folly::EventBase& eventBase() const {
    return eventBase_;
  }
  folly::EventBase& eventBase() {
    return eventBase_;
  }

 private:
  const std::string name_;
  folly::EventBase eventBase_;
  std::thread thread_;
};
}
