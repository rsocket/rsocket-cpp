#pragma once

#include <atomic>
#include <type_traits>

namespace yarpl {

class Refcounted {
 public:
  virtual ~Refcounted() = default;

 private:
  template <typename T, typename>
  friend class Reference;

  void incRef() {
    refcount_.fetch_add(1, std::memory_order_relaxed);
  }

  void decRef() {
    if (refcount_.fetch_sub(1, std::memory_order_relaxed) == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      delete this;
    }
  }

  mutable std::atomic_int refcount_{0};
};

template <
    typename T,
    typename =
        typename std::enable_if<std::is_base_of<Refcounted, T>::value>::type>
class Reference {
 public:
  Reference() : pointer_(nullptr) {}

  Reference(T* pointer) : pointer_(pointer) {
    if (pointer_)
      pointer_->incRef();
  }

  ~Reference() {
    if (pointer_)
      pointer_->decRef();
  }

  template <typename U, typename>
  friend class Reference;

  template <typename U>
  Reference(Reference<U>&& other) : pointer_(other.pointer_) {
    other.pointer_ = nullptr;
  }

  template <typename U>
  Reference& operator=(Reference<U>&& other) {
    Reference(static_cast<Reference<U>&&>(other)).swap(*this);
    return *this;
  }

  Reference& operator=(Reference&& other) {
    Reference(static_cast<Reference&&>(other)).swap(*this);
    return *this;
  }

  Reference& operator=(const Reference& other) {
    Reference(other.pointer_).swap(*this);
    return *this;
  }

  Reference(const Reference& other) : pointer_(other.pointer_) {
    if (pointer_)
      pointer_->incRef();
  }

  T* get() const {
    return pointer_;
  }

  T& operator*() const {
    return *pointer_;
  }

  T* operator->() const {
    return pointer_;
  }

  void reset() {
    Reference().swap(*this);
  }

 private:
  void swap(Reference& other) {
    T* temp = pointer_;
    pointer_ = other.pointer_;
    other.pointer_ = temp;
  }

  T* pointer_;
};

} // yarpl
