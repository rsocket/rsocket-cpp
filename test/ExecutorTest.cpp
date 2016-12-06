// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>
#include <random>
#include "src/SubscriberBase.h"

using namespace ::testing;
using namespace ::reactivesocket;

class ExecutorBaseTest : public ExecutorBase {
 public:
  using ExecutorBase::ExecutorBase;
  using ExecutorBase::runInExecutor;
};

TEST(ExecutorTest, TasksQueued) {
  static const int threadNum = 10;
  static const int tasksNum = 20;
  std::atomic<int> executed{0};

  std::array<folly::ScopedEventBaseThread, threadNum> threads;

  ExecutorBaseTest executorBase(defaultExecutor(), /*start=*/false);

  auto queueItems = [&] {
    for (int i = 0; i < tasksNum; i++) {
      executorBase.runInExecutor([&] { ++executed; });
      std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 10));
    }
  };

  for (auto& thread : threads) {
    thread.getEventBase()->runInEventBaseThread(queueItems);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 10));
  executorBase.start();

  while (executed < threadNum * tasksNum)
    ;
}

TEST(ExecutorTest, TasksExecutedInOrder) {
  static const int tasksNum = 20;
  std::atomic<int> executed{0};

  folly::ScopedEventBaseThread thread2;

  ExecutorBaseTest executorBase(defaultExecutor(), /*start=*/false);

  auto queueItems = [&] {
    for (int i = 0; i < tasksNum; i++) {
      executorBase.runInExecutor([&, i] { EXPECT_EQ(i, executed++); });
      std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 10));
    }
  };

  thread2.getEventBase()->runInEventBaseThread(queueItems);

  std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 10));
  executorBase.start();

  while (executed < tasksNum)
    ;
}
