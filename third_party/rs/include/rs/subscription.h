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

#include <limits>
#include <memory>
#include <type_traits>

#include <rs/detail/optional.h>
#include <rs/element_count.h>

namespace shk {

/**
 * Classes that conform to the Subscription concept should inherit from this
 * class to signify that they are a Subscription.
 *
 * Subscription types must have the following methods:
 *
 * * void Request(ElementCount count);
 * * void Cancel();
 *
 * Destroying a Subscription object implicitly cancels the subscription.
 */
class Subscription {
 protected:
  ~Subscription() = default;
};

namespace detail {

class EmptySubscription : public Subscription {
 public:
  EmptySubscription() = default;

  EmptySubscription(const EmptySubscription &) = delete;
  EmptySubscription& operator=(const EmptySubscription &) = delete;

  EmptySubscription(EmptySubscription &&) = default;
  EmptySubscription& operator=(EmptySubscription &&) = default;

  void Request(ElementCount count);
  void Cancel();
};


template <typename RequestCb, typename CancelCb>
class CallbackSubscription : public Subscription {
 public:
  CallbackSubscription() = default;

  template <typename RequestCbT, typename CancelCbT>
  CallbackSubscription(RequestCbT &&request, CancelCbT &&cancel)
      : request_(std::forward<RequestCbT>(request)),
        cancel_(std::forward<CancelCbT>(cancel)) {}

  CallbackSubscription(const CallbackSubscription &) = delete;
  CallbackSubscription &operator=(const CallbackSubscription &) = delete;

  CallbackSubscription(CallbackSubscription &&) = default;
  CallbackSubscription &operator=(CallbackSubscription &&) = default;

  void Request(ElementCount count) {
    if (request_) {
      (*request_)(count);
    }
  }

  void Cancel() {
    if (cancel_) {
      (*cancel_)();
    }
  }

 private:
  detail::Optional<RequestCb> request_;
  detail::Optional<CancelCb> cancel_;
};

}  // namespace detail

/**
 * This class is a pure virtual class version of the Subscription concept. It is
 * only useful in some very specific use cases; this is not the main
 * Subscription interface.
 *
 * Any Subscription object can be turned into a PureVirtualSubscription with the
 * help of the VirtualSubscription wrapper class.
 */
class PureVirtualSubscription : public Subscription {
 public:
  virtual ~PureVirtualSubscription();
  virtual void Request(ElementCount count) = 0;
  virtual void Cancel() = 0;
};

/**
 * Helper class that wraps any Subscription object without changing its
 * behavior. What it adds is that it implements the PureVirtualSubscription
 * interface, which is useful for example when implementing AnySubscription but
 * it can also be useful to operator implementations. See for example Map and
 * Filter.
 */
template <typename S>
class VirtualSubscription : public PureVirtualSubscription {
 public:
  VirtualSubscription() = default;

  template <typename SType>
  explicit VirtualSubscription(SType &&subscription)
      : subscription_(std::forward<SType>(subscription)) {}

  void Request(ElementCount count) override {
    subscription_.Request(count);
  }

  void Cancel() override {
    subscription_.Cancel();
  }

 private:
  // Sanity check... it's really bad if the user of this class forgets to
  // std::decay the template parameter if necessary.
  static_assert(
      !std::is_reference<S>::value,
      "Subscription type must be held by value");
  S subscription_;
};

template <typename T>
VirtualSubscription<typename std::decay<T>::type>
MakeVirtualSubscription(T &&t) {
  return VirtualSubscription<typename std::decay<T>::type>(
      std::forward<T>(t));
}

template <typename T>
std::unique_ptr<VirtualSubscription<typename std::decay<T>::type>>
MakeVirtualSubscriptionPtr(T &&t) {
  using SubscriptionType = VirtualSubscription<typename std::decay<T>::type>;
  return std::unique_ptr<SubscriptionType>(new SubscriptionType(
      std::forward<T>(t)));
}

template <typename T>
constexpr bool IsSubscription = std::is_base_of<Subscription, T>::value;

/**
 * Type erasure wrapper for Subscription objects that owns the erased
 * Subscription via unique_ptr.
 */
class AnySubscription : public Subscription {
 public:
  AnySubscription();

  /**
   * S should implement the Subscription concept.
   */
  template <
      typename S,
      class = typename std::enable_if<IsSubscription<
          typename std::remove_reference<S>::type>>::type>
  explicit AnySubscription(S &&s)
      : eraser_(std::make_unique<VirtualSubscription<
            typename std::decay<S>::type>>(std::forward<S>(s))) {}

  AnySubscription(const AnySubscription &) = delete;
  AnySubscription &operator=(const AnySubscription &) = delete;

  AnySubscription(AnySubscription &&) = default;
  AnySubscription &operator=(AnySubscription &&) = default;

  void Request(ElementCount count);
  void Cancel();

 private:
  std::unique_ptr<PureVirtualSubscription> eraser_;
};

detail::EmptySubscription MakeSubscription();

template <typename RequestCb, typename CancelCb>
auto MakeSubscription(RequestCb &&request, CancelCb &&cancel) {
  return detail::CallbackSubscription<
      typename std::decay<RequestCb>::type,
      typename std::decay<CancelCb>::type>(
          std::forward<RequestCb>(request),
          std::forward<CancelCb>(cancel));
}

}  // namespace shk
