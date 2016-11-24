#include "EventBaseExecutor.h"

#include <folly/ThreadName.h>

namespace reactivesocket {

EventBaseExecutor::EventBaseExecutor(std::string name)
    : name_(std::move(name)) {
}

void EventBaseExecutor::startThread() {
  thread_ = std::thread([this]() {
    folly::setThreadName(name_);
    eventBase_.loopForever();
  });
}

void EventBaseExecutor::add(folly::Func func) {
  eventBase_.runInEventBaseThread(std::move(func));
}

void EventBaseExecutor::scheduleAt(
    folly::Func&& func,
    const EventBaseExecutor::TimePoint& t) {
  eventBase_.runInEventBaseThread(
      [& ev = eventBase_, t, func = std::move(func) ]() mutable {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            t - std::chrono::steady_clock::now());

        if (!ev.tryRunAfterDelay(std::move(func), ms.count())) {
          LOG(ERROR) << "Failed to schedule callback after " << ms.count()
                     << "ms";
        }
      });
}

void EventBaseExecutor::prepareStopThread() {
  eventBase_.terminateLoopSoon();
}

void EventBaseExecutor::stopThread() {
  prepareStopThread();
  if (thread_.joinable()) {
    thread_.join();
  }
}

EventBaseExecutor::~EventBaseExecutor() {
  stopThread();
}
}
