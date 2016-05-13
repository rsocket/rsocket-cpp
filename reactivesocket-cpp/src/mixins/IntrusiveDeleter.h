// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cassert>
#include <cstddef>
#include <memory>

namespace lithium {
namespace reactivesocket {

/// Deletes itself using the first time the reference count, initially one,
/// drops to zero.
class IntrusiveDeleter {
 public:
  virtual ~IntrusiveDeleter() = 0;

  void incrementRefCount() {
    ++refCount_;
  }

  void decrementRefCount() {
    assert(refCount_ > 0);
    if (--refCount_ == 0) {
      // Tail-call
      delete this;
    }
  }

 private:
  size_t refCount_{1};
};

inline IntrusiveDeleter::~IntrusiveDeleter() = default;
}
}
