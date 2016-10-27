// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

namespace reactivesocket {

class EnableSharedFromThisVirtualBase
    : public std::enable_shared_from_this<EnableSharedFromThisVirtualBase> {};

template <typename T>
class EnableSharedFromThisBase
    : public virtual EnableSharedFromThisVirtualBase {
 public:
  std::shared_ptr<T> shared_from_this() {
    std::shared_ptr<T> result(
        EnableSharedFromThisVirtualBase::shared_from_this(),
        static_cast<T*>(this));
    return result;
  }

  std::shared_ptr<const T> shared_from_this() const {
    std::shared_ptr<const T> result(
        EnableSharedFromThisVirtualBase::shared_from_this(),
        static_cast<const T*>(this));
    return result;
  }
};
} // reactivesocket
