#pragma once

#include <atomic>
#include <functional>
#include <memory>

namespace yarpl {
namespace observable {

class Subscription {
  std::atomic_bool cancelled{false};
  std::function<void()> onCancel;

private:
  Subscription(std::function<void()> onCancel) : onCancel(onCancel){};

public:
  static std::unique_ptr<Subscription> create(std::function<void()> onCancel);
  static std::unique_ptr<Subscription> create(std::atomic_bool &cancelled);
  static std::unique_ptr<Subscription> create();
  void cancel();
  bool isCanceled();
};

} // observable namespace
} // yarpl namespace
