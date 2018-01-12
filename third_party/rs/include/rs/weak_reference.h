// Copyright 2017 Per Grön. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <type_traits>

namespace shk {

template <typename T>
class WeakReference;

template <typename T>
class WeakReferee;

namespace detail {

template <typename T, typename U>
WeakReferee<typename std::decay<T>::type> WithSingleWeakReference(
    T &&t, WeakReference<U> *backref) {
  WeakReferee<typename std::decay<T>::type> weak_referee(std::forward<T>(t));
  *backref = WeakReference<U>(&weak_referee);
  return weak_referee;
}

}  // namespace detail

template <typename T>
class WeakReferee : public T {
 public:
  WeakReferee()
      : T(),
        backref_(nullptr) {}

  ~WeakReferee() {
    if (backref_) {
      backref_->val_ = nullptr;
    }
  }
  using T::operator=;

  WeakReferee(const WeakReferee &) = delete;
  WeakReferee &operator=(const WeakReferee &) = delete;

  /**
   * For safety, disallow unwrapping WeakReferees. This deleted constructor
   * disallows things such as
   *
   *     WeakReferee<WeakReferee<std::string>> blah = ...;
   *     WeakReferee<std::string> bleh = std::move(blah);
   */
  template <
      typename U,
      class = typename std::enable_if<
          std::is_same<T, U>::value>::type>
  WeakReferee(WeakReferee<WeakReferee<U>> &&other) = delete;

  WeakReferee(WeakReferee &&other)
      : T(std::move(static_cast<T &>(other))),
        backref_(other.backref_) {
    other.backref_ = nullptr;
    if (backref_) {
      backref_->val_ = this;
    }
  }

  WeakReferee &operator=(WeakReferee &&other) {
    if (backref_) {
      backref_->val_ = nullptr;
    }

    T::operator=(std::move(other));
    backref_ = other.backref_;
    other.backref_ = nullptr;
    if (backref_) {
      backref_->val_ = this;
    }
    return *this;
  }

 private:
  template <typename> friend class WeakReference;
  template <typename U, typename V>
  friend WeakReferee<typename std::decay<U>::type>
  detail::WithSingleWeakReference(U &&, WeakReference<V> *);

  template <typename U>
  explicit WeakReferee(U &&value) : T(std::forward<U>(value)) {}

  WeakReference<T> *backref_ = nullptr;
};

// This class is final because I'm not sure the reinterpret_cast below works
// otherwise.
template <typename T>
class WeakReference final {
 public:
  WeakReference()
      : val_(nullptr),
        set_backref_(nullptr) {}

  ~WeakReference() {
    if (val_) {
      set_backref_(val_, nullptr);
    }
  }

  WeakReference(const WeakReference &) = delete;
  WeakReference &operator=(const WeakReference &) = delete;

  WeakReference(WeakReference &&other)
      : val_(other.val_),
        set_backref_(other.set_backref_) {
    other.val_ = nullptr;
    other.set_backref_ = nullptr;
    if (val_) {
      set_backref_(val_, this);
    }
  }

  WeakReference &operator=(WeakReference &&other) {
    if (val_) {
      set_backref_(val_, nullptr);
    }
    val_ = other.val_;
    set_backref_ = other.set_backref_;
    other.val_ = nullptr;
    other.set_backref_ = nullptr;
    if (val_) {
      set_backref_(val_, this);
    }
    return *this;
  }

  void Reset() {
    if (val_) {
      set_backref_(val_, nullptr);
    }
    val_ = nullptr;
    set_backref_ = nullptr;
  }

  explicit operator bool() const {
    return !!val_;
  }

  T &operator*() {
    return *val_;
  }

  const T &operator*() const {
    return *val_;
  }

  T *operator->() {
    return val_;
  }

  const T *operator->() const {
    return val_;
  }

 private:
  template <typename> friend class WeakReferee;
  template <typename U, typename V>
  friend WeakReferee<typename std::decay<U>::type>
  detail::WithSingleWeakReference(U &&, WeakReference<V> *);

  template <typename SubType, typename SuperType>
  static void SetBackref(SuperType *val, WeakReference *ptr) {
    static_cast<WeakReferee<SubType> *>(val)->backref_ =
        reinterpret_cast<WeakReference<SubType> *>(ptr);
  }

  template <typename U>
  explicit WeakReference(WeakReferee<U> *val)
      : val_(val),
        set_backref_(&SetBackref<U, T>) {}

  T *val_;
  void (*set_backref_)(T *val, WeakReference *ptr);
};

// TODO(peck): Document this
template <typename T>
auto WithWeakReference(T &&t) {
  return std::forward<T>(t);
}

template <typename T, typename U, typename ...V>
auto WithWeakReference(
    T &&t, WeakReference<U> *backref, WeakReference<V> *...backrefs) {
  return detail::WithSingleWeakReference(
      WithWeakReference(std::forward<T>(t), backrefs...),
      backref);
}

}  // namespace shk
