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
#include <new>
#include <algorithm>

namespace shk {
namespace detail {
namespace internal {

template<typename T, bool IsReference>
struct OptionalTypes {
};

template<typename T>
struct OptionalTypes<T, false> {
  typedef T        value_type;
  typedef T        storage_type;
  typedef T const &reference_const_type;
  typedef T       &reference_type;
  typedef T const *pointer_const_type;
  typedef T       *pointer_type;
  typedef T const &argument_type;
};

template<typename T>
struct OptionalTypes<T, true> {
  typedef typename std::remove_reference<T>::type value_type;
  typedef value_type *storage_type;
  typedef value_type  const &reference_const_type;
  typedef value_type &reference_type;
  typedef value_type  const *pointer_const_type;
  typedef value_type *pointer_type;
  typedef value_type &argument_type;
};

}  // namespace internal


/**
 * Optional is a class that encapsulates the concept of an optional value. It
 * does not allocate memory, but stores the value "by value", plus one byte for
 * keeping track of whether the value is set or not.
 *
 * Using this class is not unlike using a smart pointer; it overloads the * and
 * -> operators.
 *
 * It also has a couple of higher-level functional-like constructs for dealing
 * with the fact that the value might not be set: See map, each and ifelse.
 */
template<typename T>
class Optional {
 public:
  typedef internal::OptionalTypes<T, std::is_reference<T>::value> types;
  typedef typename types::value_type value_type;
  typedef typename types::storage_type storage_type;
  typedef typename types::reference_const_type reference_const_type;
  typedef typename types::reference_type reference_type;
  typedef typename types::pointer_const_type pointer_const_type;
  typedef typename types::pointer_type pointer_type;
  typedef typename types::argument_type argument_type;

  Optional() { SetSet(false); }
  Optional(const Optional<T> &other) {
    if (other) {
      CopyConstruct(*other.Memory());
      SetSet(true);
    } else {
      SetSet(false);
    }
  }
  Optional(Optional<T> &&other) {
    if (other) {
      MoveConstruct(std::move(*other.Memory()));
      SetSet(true);
      other.Clear();
    } else {
      SetSet(false);
    }
  }
  explicit Optional(argument_type other) {
    CopyConstruct(other);
    SetSet(true);
  }
  explicit Optional(value_type &&other) {
    MoveConstruct(std::move(other));
    SetSet(true);
  }

  ~Optional() { Clear(); }

  Optional<T>& operator=(const Optional<T> &other) {
    if (other) {
      Assign(*other.Memory());
    } else {
      Clear();
    }
    return *this;
  }

  Optional<T>& operator=(Optional<T> &&other) {
    if (other) {
      Assign(std::move(*other.Memory()));
      other.Clear();
    } else {
      Clear();
    }
    return *this;
  }

  Optional<T>& operator=(argument_type other) {
    Assign(other);
    return *this;
  }

  Optional<T>& operator=(value_type &&other) {
    Assign(std::move(other));
    return *this;
  }

  /**
   * Returns non-zero iff the object stores a value.
   */
  bool IsSet() const {
    return data_[sizeof(storage_type)];
  }

  /**
   * Bool conversion operator, for use in if statements etc.
   * This is an alias for isSet.
   */
  explicit operator bool() const {
    return IsSet();
  }

  /**
   * Returns a pointer to the object stored, or nullptr if not set.
   */
  pointer_type Get() { return IsSet() ? Memory() : nullptr; }
  pointer_const_type Get() const { return IsSet() ? Memory() : nullptr; }

  /**
   * This is a convenience operator in order to be able to use the
   * object as you would a smart pointer. Returns nullptr if not set.
   */
  pointer_type operator->() { return Get(); }
  pointer_const_type operator->() const { return Get(); }

  /**
   * This is a convenience operator in order to be able to use the
   * object as you would a smart pointer. Return value is undefined
   * if not set.
   */
  reference_type operator*() { return *Get(); }
  reference_const_type operator*() const { return *Get(); }

  /**
   * If the object stores a value, clear it. If the
   * object is not set, this is a no-op.
   */
  void Clear() {
    if (IsSet()) {
      InvokeDestructor();
      SetSet(false);
    }
  }

  /**
   * Takes a functor and invokes it with the object, and returns an optional of
   * the return value of the functor, if set. If not set, returns an empty
   * optional of the functor's return type.
   */
  template<typename Functor>
  auto Map(Functor &&functor)
  -> Optional<decltype(functor(*static_cast<pointer_type>(nullptr)))> {
    typedef decltype(functor(*static_cast<pointer_type>(nullptr))) ReturnType;
    if (IsSet()) {
      return Optional<ReturnType>(functor(*Get()));
    } else {
      return Optional<ReturnType>();
    }
  }

  /**
   * Takes a functor and invokes it with the object if set. Otherwise, this
   * is a no-op.
   */
  template<typename Functor>
  void Each(Functor &&functor) {
    if (IsSet()) {
      functor(*Get());
    }
  }

  /**
   * Takes a functor and invokes it with the object if set. Otherwise, this
   * is a no-op.
   */
  template<typename Functor>
  void Each(Functor &&functor) const {
    if (IsSet()) {
      functor(*Get());
    }
  }

  /**
   * Takes two functors. The first one is called if the object is set, the
   * second one if it isn't. It returns what the invoked functor returns.
   */
  template<typename FunctorIf, typename FunctorElse>
  auto IfElse(FunctorIf &&functorIf, FunctorElse &&functorElse)
  -> decltype(functorIf(*static_cast<pointer_type>(nullptr))) {
    if (IsSet()) {
      return functorIf(*Get());
    } else {
      return functorElse();
    }
  }

 private:
  template<typename U = T>
  typename std::enable_if<std::is_reference<U>::value>::type
  InvokeDestructor() {}

  template<typename U = T>
  typename std::enable_if<!std::is_reference<U>::value>::type
  InvokeDestructor() {
    Memory()->~T();
  }

  template<typename U = T>
  typename std::enable_if<std::is_reference<U>::value, pointer_type>::type
  Memory() {
    return *reinterpret_cast<value_type**>(data_);
  }

  template<typename U = T>
  typename std::enable_if<!std::is_reference<U>::value,
                          pointer_type>::type
  Memory() {
    return reinterpret_cast<pointer_type>(&data_);
  }

  template<typename U = T>
  typename std::enable_if<std::is_reference<U>::value,
                          pointer_const_type>::type
  Memory() const {
    return *reinterpret_cast<value_type* const*>(data_);
  }

  template<typename U = T>
  typename std::enable_if<!std::is_reference<U>::value,
                          pointer_const_type>::type
  Memory() const {
    return reinterpret_cast<pointer_const_type>(&data_);
  }

  void SetSet(bool set) {
    data_[sizeof(storage_type)] = set;
  }

  template<typename U = T>
  typename std::enable_if<std::is_reference<U>::value>::type
  CopyConstruct(argument_type val) {
    *reinterpret_cast<value_type**>(data_) = &val;
  }

  template<typename U = T>
  typename std::enable_if<!std::is_reference<U>::value>::type
  CopyConstruct(argument_type val) {
    ::new(Memory()) T(val);
  }

  template<typename U = T>
  typename std::enable_if<std::is_reference<U>::value>::type
  MoveConstruct(value_type&& val) {
    *reinterpret_cast<value_type**>(data_) = &val;
  }

  template<typename U = T>
  typename std::enable_if<!std::is_reference<U>::value>::type
  MoveConstruct(value_type&& val) {
    ::new(Memory()) T(std::move(val));
  }

  template<typename U = T>
  typename std::enable_if<std::is_copy_constructible<U>::value &&
                          std::is_copy_assignable<U>::value>::type
  Assign(argument_type by_copy) {
    if (IsSet()) {
      *Memory() = by_copy;
    } else {
      CopyConstruct(by_copy);
      SetSet(true);
    }
  }

  template<typename U = T>
  typename std::enable_if<std::is_copy_constructible<U>::value &&
                          !std::is_copy_assignable<U>::value>::type
  Assign(argument_type by_copy) {
    if (IsSet()) {
      Clear();
    }
    CopyConstruct(by_copy);
    SetSet(true);
  }

  template<typename U = T>
  typename std::enable_if<!std::is_copy_constructible<U>::value &&
                          std::is_copy_assignable<U>::value>::type
  Assign(argument_type by_copy) {
    if (!IsSet()) {
      ::new(Memory()) T;
      SetSet(true);
    }
    *Memory() = by_copy;
  }

  template<typename U = T>
  typename std::enable_if<std::is_move_constructible<U>::value &&
                          std::is_move_assignable<U>::value>::type
  Assign(value_type &&by_move) {
    if (IsSet()) {
      *Memory() = std::move(by_move);
    } else {
      MoveConstruct(std::move(by_move));
      SetSet(true);
    }
  }

  template<typename U = T>
  typename std::enable_if<std::is_move_constructible<U>::value &&
                          !std::is_move_assignable<U>::value>::type
  Assign(value_type &&by_move) {
    if (IsSet()) {
      Clear();
    }
    MoveConstruct(std::move(by_move));
    SetSet(true);
  }

  template<typename U = T>
  typename std::enable_if<!std::is_move_constructible<U>::value &&
                          std::is_move_assignable<U>::value>::type
  Assign(value_type &&by_move) {
    if (!IsSet()) {
      ::new(Memory()) T;
      SetSet(true);
    }
    *Memory() = std::move(by_move);
  }

  /**
   * The last byte of this char array is non-zero if
   * data_ contains an initialized value.
   */
  alignas(T) char data_[sizeof(storage_type)+1];
};


// Optional vs Optional

template<typename T>
bool operator==(const Optional<T> &x, const Optional<T> &y) {
  return ((!x && !y) ||
          (x && y && *x == *y));
}

template<typename T>
bool operator<(const Optional<T> &x, const Optional<T> &y) {
  if (!y) {
    return false;
  } else if (!x) {
    return true;
  } else {
    return (*x < *y);
  }
}

template<typename T>
bool operator!=(const Optional<T> &x, const Optional<T> &y) {
  return !(x == y);
}

template<typename T>
bool operator>(const Optional<T> &x, const Optional<T> &y) {
  return y < x;
}

template<typename T>
bool operator<=(const Optional<T> &x, const Optional<T> &y) {
  return !(x > y);
}

template<typename T>
bool operator>=(const Optional<T> &x, const Optional<T> &y) {
  return !(x < y);
}


// T vs Optional

template<typename T>
bool operator==(const typename std::remove_reference<T>::type &x,
                const Optional<T> &y) {
  return (y && x == *y);
}

template<typename T>
bool operator<(const typename std::remove_reference<T>::type &x,
               const Optional<T> &y) {
  if (!y) {
    return false;
  } else {
    return (x < *y);
  }
}

template<typename T>
bool operator!=(const typename std::remove_reference<T>::type &x,
                const Optional<T> &y) {
  return !(x == y);
}

template<typename T>
bool operator>(const typename std::remove_reference<T>::type &x,
               const Optional<T> &y) {
  return y < x;
}

template<typename T>
bool operator<=(const typename std::remove_reference<T>::type &x,
                const Optional<T> &y) {
  return !(x > y);
}

template<typename T>
bool operator>=(const typename std::remove_reference<T>::type &x,
                const Optional<T> &y) {
  return !(x < y);
}


// Optional vs T

template<typename T>
bool operator==(const Optional<T> &x,
                const typename std::remove_reference<T>::type &y) {
  return (x && *x == y);
}

template<typename T>
bool operator<(const Optional<T> &x,
               const typename std::remove_reference<T>::type &y) {
  if (!x) {
    return true;
  } else {
    return (*x < y);
  }
}

template<typename T>
bool operator!=(const Optional<T> &x,
                const typename std::remove_reference<T>::type &y) {
  return !(x == y);
}

template<typename T>
bool operator>(const Optional<T> &x,
               const typename std::remove_reference<T>::type &y) {
  return y < x;
}

template<typename T>
bool operator<=(const Optional<T> &x,
                const typename std::remove_reference<T>::type &y) {
  return !(x > y);
}

template<typename T>
bool operator>=(const Optional<T> &x,
                const typename std::remove_reference<T>::type &y) {
  return !(x < y);
}

}  // namespace detail
}  // namespace shk


namespace std {

template<typename T>
void swap(shk::detail::Optional<T> &lhs, shk::detail::Optional<T> &rhs) {
  if (!lhs && !rhs) {
    // No-op
  } else if (!rhs) {
    rhs = std::move(lhs);
  } else if (!lhs) {
    lhs = std::move(rhs);
  } else {
    std::swap(*lhs, *rhs);
  }
}

}  // namespace std
