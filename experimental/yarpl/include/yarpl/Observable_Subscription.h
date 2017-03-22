// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include "yarpl/utils/type_traits.h"

namespace yarpl {
namespace observable {

class Subscription {
 public:
  explicit Subscription(std::function<void()> onCancel)
      : onCancel_(std::move(onCancel)) {}

  static std::unique_ptr<Subscription> create(std::function<void()> onCancel);
  static std::unique_ptr<Subscription> create(std::atomic_bool& cancelled);
  static std::unique_ptr<Subscription> create();

  void cancel();
  bool isCanceled() const;

 private:
  std::atomic_bool cancelled_{false};
  std::function<void()> onCancel_;
};

} // observable namespace
} // yarpl namespace
