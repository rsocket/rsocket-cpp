// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace yarpl {

namespace detail {
struct skip_initial_refcount_check {};
struct do_initial_refcount_check {};
}

template <typename T>
class Reference;

/// Base of refcounted objects.  The intention is the same as that
/// of boost::intrusive_ptr<>, except that we have virtual methods
/// anyway, and want to avoid argument-dependent lookup.
///
/// NOTE: Only derive using "virtual public" inheritance.
class Refcounted {
 public:
  virtual ~Refcounted() = default;

  // Return the current count.  For testing.
  std::size_t count() const {
    return refcount_;
  }

 private:
  template <typename U>
  friend class Reference;

  void incRef() {
    refcount_.fetch_add(1, std::memory_order_relaxed);
  }

  void decRef() {
    auto previous = refcount_.fetch_sub(1, std::memory_order_relaxed);
    assert(previous >= 1 && "decRef on a destroyed object!");
    if (previous == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      delete this;
    }
  }

  // refcount starts at 1 always, so we don't destroy ourselves in
  // the constructor if we call `get_ref` in it
  mutable std::atomic_size_t refcount_{1};
};

/// RAII-enabling smart pointer for refcounted objects.  Each reference
/// constructed against a target refcounted object increases its count by 1
/// during its lifetime.
template <typename T>
class Reference {
 public:
  template <typename U>
  friend class Reference;

  Reference() = default;
  inline /* implicit */ Reference(std::nullptr_t) {}

  explicit Reference(T* pointer, detail::skip_initial_refcount_check)
      : pointer_(pointer) {
    // newly constructed object in `make_ref` already had a refcount of 1,
    // so don't increment it (we take 'ownership' of the reference made in
    // make_ref)
    assert(pointer->Refcounted::count() >= 1);
  }
  explicit Reference(T* pointer, detail::do_initial_refcount_check)
      : pointer_(pointer) {
    /**
     * consider the following:
     *
     class MyClass : Refcounted {
       MyClass() {
        // count() == 0
        auto r = get_ref<MyClass>(this)
        // count() == 1
        do_something_with(r);
        // if do_something_with(r) doens't keep a reference to r somewhere, then
        // count() == 0
        // and we call ~MyClass() within the constructor, which is a Bad Thing
       }
     };

     * the check below prevents (at runtime) taking a reference in situations
     * like this
     */
    assert(
        pointer->count() >= 1 &&
        "can't take an additional reference to something with a zero refcount");
    inc();
  }

  ~Reference() {
    dec();
  }

  //////////////////////////////////////////////////////////////////////////////

  Reference(const Reference& other) : pointer_(other.pointer_) {
    inc();
  }

  Reference(Reference&& other) noexcept : pointer_(other.pointer_) {
    other.pointer_ = nullptr;
  }

  template <typename U>
  Reference(const Reference<U>& other) : pointer_(other.pointer_) {
    inc();
  }

  template <typename U>
  Reference(Reference<U>&& other) : pointer_(other.pointer_) {
    other.pointer_ = nullptr;
  }

  //////////////////////////////////////////////////////////////////////////////

  Reference& operator=(std::nullptr_t) {
    reset();
    return *this;
  }

  Reference& operator=(const Reference& other) {
    return assign(other);
  }

  Reference& operator=(Reference&& other) {
    return assign(std::move(other));
  }

  template <typename U>
  Reference& operator=(const Reference<U>& other) {
    return assign(other);
  }

  template <typename U>
  Reference& operator=(Reference<U>&& other) {
    return assign(std::move(other));
  }

  //////////////////////////////////////////////////////////////////////////////

  // TODO: remove this from public Reference API
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
    Reference{}.swap(*this);
  }

  explicit operator bool() const {
    return pointer_;
  }

 private:
  void inc() {
    static_assert(
        std::is_base_of<Refcounted, T>::value,
        "Reference must be used with types that virtually derive Refcounted");

    if (pointer_) {
      pointer_->incRef();
    }
  }

  void dec() {
    static_assert(
        std::is_base_of<Refcounted, T>::value,
        "Reference must be used with types that virtually derive Refcounted");

    if (pointer_) {
      pointer_->decRef();
    }
  }

  void swap(Reference<T>& other) {
    std::swap(pointer_, other.pointer_);
  }

  template <typename Ref>
  Reference& assign(Ref&& other) {
    Reference<T> temp(std::forward<Ref>(other));
    swap(temp);
    return *this;
  }

  T* pointer_{nullptr};
};

template <typename T, typename U>
bool operator==(const Reference<T>& lhs, const Reference<U>& rhs) noexcept {
  return lhs.get() == rhs.get();
}

template <typename T>
bool operator==(const Reference<T>& lhs, std::nullptr_t) noexcept {
  return lhs.get() == nullptr;
}

template <typename T>
bool operator==(std::nullptr_t, const Reference<T>& rhs) noexcept {
  return rhs.get() == nullptr;
}

template <typename T, typename U>
bool operator!=(const Reference<T>& lhs, const Reference<U>& rhs) noexcept {
  return lhs.get() != rhs.get();
}

template <typename T>
bool operator!=(const Reference<T>& lhs, std::nullptr_t) noexcept {
  return lhs.get() != nullptr;
}

template <typename T>
bool operator!=(std::nullptr_t, const Reference<T>& rhs) noexcept {
  return rhs.get() != nullptr;
}

template <typename T, typename U>
bool operator<(const Reference<T>& lhs, const Reference<U>& rhs) noexcept {
  return lhs.get() < rhs.get();
}

template <typename T>
bool operator<(const Reference<T>& lhs, std::nullptr_t) noexcept {
  return lhs.get() < nullptr;
}

template <typename T>
bool operator<(std::nullptr_t, const Reference<T>& rhs) noexcept {
  return nullptr < rhs.get();
}

template <typename T, typename U>
bool operator<=(const Reference<T>& lhs, const Reference<U>& rhs) noexcept {
  return lhs.get() <= rhs.get();
}

template <typename T>
bool operator<=(const Reference<T>& lhs, std::nullptr_t) noexcept {
  return lhs.get() <= nullptr;
}

template <typename T>
bool operator<=(std::nullptr_t, const Reference<T>& rhs) noexcept {
  return nullptr <= rhs.get();
}

template <typename T, typename U>
bool operator>(const Reference<T>& lhs, const Reference<U>& rhs) noexcept {
  return lhs.get() > rhs.get();
}

template <typename T>
bool operator>(const Reference<T>& lhs, std::nullptr_t) noexcept {
  return lhs.get() > nullptr;
}

template <typename T>
bool operator>(std::nullptr_t, const Reference<T>& rhs) noexcept {
  return nullptr > rhs.get();
}

template <typename T, typename U>
bool operator>=(const Reference<T>& lhs, const Reference<U>& rhs) noexcept {
  return lhs.get() >= rhs.get();
}

template <typename T>
bool operator>=(const Reference<T>& lhs, std::nullptr_t) noexcept {
  return lhs.get() >= nullptr;
}

template <typename T>
bool operator>=(std::nullptr_t, const Reference<T>& rhs) noexcept {
  return nullptr >= rhs.get();
}

////////////////////////////////////////////////////////////////////////////////

template <typename T, typename CastTo = T, typename... Args>
Reference<CastTo> make_ref(Args&&... args) {
  static_assert(
      std::is_base_of<Refcounted, std::decay_t<T>>::value,
      "Reference can only be constructed with a Refcounted object");

  static_assert(
      std::is_base_of<std::decay_t<CastTo>, std::decay_t<T>>::value,
      "Concrete type must be a subclass of casted-to-type");

  return Reference<CastTo>(
    new T(std::forward<Args>(args)...),
    detail::skip_initial_refcount_check{}
  );
}

template <typename T>
Reference<T> get_ref(T& object) {
  static_assert(
      std::is_base_of<Refcounted, std::decay_t<T>>::value,
      "Reference can only be constructed with a Refcounted object");

  return Reference<T>(&object, detail::do_initial_refcount_check{});
}

template <typename T>
Reference<T> get_ref(T* object) {
  static_assert(
      std::is_base_of<Refcounted, std::decay_t<T>>::value,
      "Reference can only be constructed with a Refcounted object");

  return Reference<T>(object, detail::do_initial_refcount_check{});
}

} // namespace yarpl

//
// custom specialization of std::hash<yarpl::Reference<T>>
//
namespace std
{
template<typename T>
struct hash<yarpl::Reference<T>>
{
  typedef yarpl::Reference<T> argument_type;
  typedef typename std::hash<T*>::result_type result_type;

  result_type operator()(argument_type const& s) const
  {
    return std::hash<T*>()(s.get());
  }
};
}
