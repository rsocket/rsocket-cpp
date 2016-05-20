// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cassert>
#include <cstddef>
#include <memory>

namespace reactivestreams {

/// Deletes specified object using std::default_delete<T> the first time
/// reference count drops to zero.
///
/// It is safe for RefCountedDeleter to be destroyed in T::~T that it invokes.
template <typename T>
class RefCountedDeleter {
 public:
  explicit RefCountedDeleter(T* object, size_t initialRefCount = 0)
      : object_(object), refCount_(initialRefCount) {}

  void increment() {
    ++refCount_;
  }

  void decrement() {
    assert(refCount_ > 0);
    if (--refCount_ == 0 && object_) {
      auto object = object_;
      object_ = nullptr;
      // Tail-call
      std::default_delete<T>()(object);
    }
  }

  /// Decrements a reference count and potentially removes the object when
  /// returned handle falls out of the scope.
  ///
  /// Example use to implement self-managed Subscription:
  ///
  ///   RefCountedDeleter<...> refCount_;
  ///   void ...::cancel() {
  ///     auto handle = refCount_.decrementDeferred();
  ///     // Perform some other, arbitrarily complicated cleanup.
  ///     ...
  ///   } // The reference count will be decremented once the method exits.
  ///
  class Handle;
  Handle decrementDeferred();

 private:
  T* object_;
  size_t refCount_;
};

template <typename T>
class RefCountedDeleter<T>::Handle {
 public:
  Handle() = default;

  Handle(Handle&& other) noexcept : refCount_(other.release()) {}
  Handle& operator=(Handle&& other) noexcept {
    refCount_ = other.release();
  }

  void decrement() {
    if (auto refCount = release()) {
      // Tail-call
      refCount->decrement();
    }
  }

  ~Handle() {
    decrement();
  }

 private:
  friend RefCountedDeleter;

  RefCountedDeleter* refCount_{nullptr};

  explicit Handle(RefCountedDeleter& refCount) : refCount_(&refCount) {}

  RefCountedDeleter* release() {
    auto refCount = refCount_;
    refCount_ = nullptr;
    return refCount;
  }
};

template <typename T>
typename RefCountedDeleter<T>::Handle
RefCountedDeleter<T>::decrementDeferred() {
  return Handle(*this);
}
}
