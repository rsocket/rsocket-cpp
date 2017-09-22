// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include <cstdlib>
#include <cxxabi.h>
#include <typeinfo>
#include <unordered_map>
#include <string>
#include <ostream>

namespace yarpl {

namespace detail {
struct skip_initial_refcount_check {};
struct do_initial_refcount_check {};


// refcount debugging utilities
using refcount_map_type = std::unordered_map<std::string, int64_t>;
void inc_created(std::string const&);

void inc_live(std::string const&);
void dec_live(std::string const&);
void debug_refcounts(std::ostream& o);

template<typename T>
std::string type_name()
{
    int status;
    std::string tname = typeid(T).name();
    char *demangled_name = abi::__cxa_demangle(tname.c_str(), NULL, NULL, &status);
    if (status == 0) {
        tname = demangled_name;
        std::free(demangled_name);
    }
    return tname;
}

#ifdef YARPL_REFCOUNT_DEBUGGING
struct set_reference_name;
#endif

} /* namespace detail */

template <typename T>
class Reference;

template <typename T>
class AtomicReference;

/// Base of refcounted objects.  The intention is the same as that
/// of boost::intrusive_ptr<>, except that we have virtual methods
/// anyway, and want to avoid argument-dependent lookup.
///
/// NOTE: Only derive using "virtual public" inheritance.
class Refcounted {
 public:

  /// dtor is thread safe because we cast thread_fence before
  /// calling delete this
  virtual ~Refcounted() = default;

  // Return the current count.  For testing.
  std::size_t count() const {
    return refcount_;
  }

 private:
  template <typename U>
  friend class Reference;
  template <typename U>
  friend class AtomicReference;

#ifdef YARPL_REFCOUNT_DEBUGGING
  friend struct detail::set_reference_name;
#endif

  void incRef() const {
    refcount_.fetch_add(1, std::memory_order_relaxed);
  }

  void decRef() const {
    auto previous = refcount_.fetch_sub(1, std::memory_order_relaxed);
    assert(previous >= 1 && "decRef on a destroyed object!");
    if (previous == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
#ifdef YARPL_REFCOUNT_DEBUGGING
      detail::dec_live(this->demangled_name_);
#endif
      delete this;
    }
  }

  // refcount starts at 1 always, so we don't destroy ourselves in
  // the constructor if we call `ref_from_this` in it
  mutable std::atomic_size_t refcount_{1};

#ifdef YARPL_REFCOUNT_DEBUGGING
  // for memory debugging and instrumentation
  std::string demangled_name_;
#endif
};

#ifdef YARPL_REFCOUNT_DEBUGGING
namespace detail {
struct set_reference_name {
  set_reference_name(std::string name, Refcounted& refcounted) {
    refcounted.demangled_name_ = std::move(name);
  }
};
}
#endif

/// RAII-enabling smart pointer for refcounted objects.  Each reference
/// constructed against a target refcounted object increases its count by 1
/// during its lifetime.
template <typename T>
class Reference final {
 public:
  template <typename U>
  friend class Reference;

  template <typename U>
  friend class AtomicReference;

  Reference() = default;
  inline /* implicit */ Reference(std::nullptr_t) {}

  explicit Reference(T* pointer, detail::skip_initial_refcount_check)
      : pointer_(pointer) {
    // newly constructed object in `make_ref` already had a refcount of 1,
    // so don't increment it (we take 'ownership' of the reference made in
    // make_ref)
    if(pointer) {
      assert(pointer->Refcounted::count() >= 1);
    }
  }
  explicit Reference(T* pointer, detail::do_initial_refcount_check)
      : pointer_(pointer) {
    /**
     * consider the following:
     *
     class MyClass : Refcounted {
       MyClass() {
        // count() == 0
        auto r = ref_from_this<MyClass>(this)
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
    if(pointer) {
      assert(
          pointer->Refcounted::count() >= 1 &&
          "can't take an additional reference to something with a zero refcount");
    }
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

  Reference(AtomicReference<T> const& other) : pointer_(other.pointer_) {
    inc();
  }

  Reference(AtomicReference<T>&& other) {
    pointer_ = other.pointer_.exchange(nullptr);
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

template <typename T>
class AtomicReference final {
  template <typename U>
  friend class AtomicReference;
  template <typename U>
  friend class Reference;

 public:
  AtomicReference() = default;
  AtomicReference(Reference<T> const& other) : pointer_(other.pointer_) {
    inc();
  }
  AtomicReference(AtomicReference<T> const& other) : pointer_(other.pointer_.load()) {
    inc();
  }

  template <typename U>
  AtomicReference(Reference<U>&& other) : pointer_(other.pointer_) {
    other.pointer_ = nullptr;
  }

  ~AtomicReference() {
    dec();
  }

  template <typename U>
  AtomicReference& operator=(Reference<U>&& other) {
    return assign(std::move(other));
  }

  Reference<T> load() {
    return Reference<T>(*this);
  }

  Reference<T> exchange(Reference<T> other) {
    T* old = pointer_.exchange(other.get());
    inc();
    Reference<T> r{old, detail::skip_initial_refcount_check{}};
    return r;
  }

  void store(Reference<T> const& other) {
    dec();
    pointer_.store(other.get());
    inc();
  }

  explicit operator bool() const {
    return pointer_;
  }

  T* operator->() const {
    return pointer_;
  }

 private:
  template <typename U>
  AtomicReference& assign(AtomicReference<U>&& other) {
    other.pointer_.store(pointer_.exchange(other.pointer_.load()));
    return *this;
  }

  template <typename U>
  AtomicReference& assign(Reference<U>&& other) {
    AtomicReference<U> atomic_other{std::forward<Reference<U>>(other)};
    return assign(std::move(atomic_other));
  }

  void inc() {
    static_assert(
        std::is_base_of<Refcounted, T>::value,
        "Reference must be used with types that virtually derive Refcounted");

    if (auto p = pointer_.load()) {
      p->incRef();
    }
  }

  void dec() {
    static_assert(
        std::is_base_of<Refcounted, T>::value,
        "Reference must be used with types that virtually derive Refcounted");

    if (auto p = pointer_.load()) {
      p->decRef();
    }
  }

  std::atomic<T*> pointer_{nullptr};
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

  auto r = Reference<CastTo>(
    new T(std::forward<Args>(args)...),
    detail::skip_initial_refcount_check{}
  );

#ifdef YARPL_REFCOUNT_DEBUGGING
  auto demangled_name = detail::type_name<T>();
  detail::inc_created(demangled_name);
  detail::inc_live(demangled_name);
  detail::set_reference_name{std::move(demangled_name), *r};
#endif

  return std::move(r);
}

class enable_get_ref {
 private:
#ifdef DEBUG
  // force the class to be polymorphic so we can dynamic_cast<T>(this)
  virtual void dummy_internal_get_ref() {}
#endif

 protected:
  // materialize a reference to 'this', but a type even further derived from
  // Derived, because C++ doesn't have covariant return types on methods
  template <typename As>
  Reference<As> ref_from_this(As* ptr) {
    // at runtime, ensure that the most derived class can indeed be
    // converted into an 'as'
    (void) ptr; // silence 'unused parameter' errors in Release builds
#ifdef DEBUG
    assert(
        dynamic_cast<As*>(this) && "must be able to convert from this to As*");
#endif

    assert(
        static_cast<As*>(this) == ptr &&
        "must give 'this' as argument to ref_from_this(this)");

    static_assert(
        std::is_base_of<Refcounted, As>::value,
        "Inferred type must be a subclass of Refcounted");

    return Reference<As>(
        static_cast<As*>(this), detail::do_initial_refcount_check{});
  }

  template <typename As>
  Reference<As const> ref_from_this(As const* ptr) const {
    (void) ptr; // silence 'unused parameter' errors in Release builds
#ifdef DEBUG
    assert(
        dynamic_cast<As const*>(this) && "must be able to convert from this to As*");
#endif

    assert(
        static_cast<As const*>(this) == ptr &&
        "must give 'this' as argument to ref_from_this(this)");

    static_assert(
        std::is_base_of<Refcounted, As>::value,
        "Inferred type must be a subclass of Refcounted");

    return Reference<As const>(
        static_cast<As const*>(this), detail::do_initial_refcount_check{});
  }
};

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
