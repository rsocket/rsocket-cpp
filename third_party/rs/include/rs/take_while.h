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

#include <rs/element_count.h>
#include <rs/publisher.h>
#include <rs/subscriber.h>
#include <rs/subscription.h>
#include <rs/weak_reference.h>

namespace shk {
namespace detail {

template <typename InnerSubscriberType, typename Predicate>
class TakeWhileSubscriber : public Subscriber {
 public:
  TakeWhileSubscriber(
      InnerSubscriberType &&inner_subscriber,
      const Predicate &predicate)
      : inner_subscriber_(std::move(inner_subscriber)),
        predicate_(predicate) {}

  void TakeSubscription(WeakReference<PureVirtualSubscription> &&subscription) {
    subscription_ = std::move(subscription);
  }

  template <typename T, class = RequireRvalue<T>>
  void OnNext(T &&t) {
    if (cancelled_) {
      return;
    }

    bool predicate_match = false;
    try {
      predicate_match = predicate_(static_cast<const T &>(t));
    } catch (...) {
      Cancel();
      inner_subscriber_.OnError(std::current_exception());
    }
    if (!cancelled_) { // If predicate_ threw, cancelled_ could be true here
      if (predicate_match) {
        inner_subscriber_.OnNext(std::forward<T>(t));
      } else {
        inner_subscriber_.OnComplete();
        Cancel();
      }
    }
  }

  void OnError(std::exception_ptr &&error) {
    if (!cancelled_) {
      inner_subscriber_.OnError(std::move(error));
    }
  }

  void OnComplete() {
    if (!cancelled_) {
      inner_subscriber_.OnComplete();
    }
  }

  void Cancel() {
    if (subscription_) {
      // If !subscription_, then the underlying subscription has been
      // destroyed and is by definition already cancelled so there is nothing
      // to do.
      subscription_->Cancel();
    }
    cancelled_ = true;
  }

 private:
  bool cancelled_ = false;
  InnerSubscriberType inner_subscriber_;
  WeakReference<PureVirtualSubscription> subscription_;
  Predicate predicate_;
};

}  // namespace detail

template <typename Predicate>
auto TakeWhile(Predicate &&predicate) {
  // Return an operator (it takes a Publisher and returns a Publisher)
  return [predicate = std::forward<Predicate>(predicate)](auto source) {
    // Return a Publisher
    return MakePublisher([predicate, source = std::move(source)](
        auto &&subscriber) {
      using TakeWhileSubscriberT = detail::TakeWhileSubscriber<
          typename std::decay<decltype(subscriber)>::type,
          typename std::decay<Predicate>::type>;

      WeakReference<TakeWhileSubscriberT> take_while_ref;
      auto take_while_subscriber = WithWeakReference(
          TakeWhileSubscriberT(
              std::forward<decltype(subscriber)>(subscriber),
              predicate),
          &take_while_ref);

      WeakReference<PureVirtualSubscription> sub_ref;
      auto sub = WithWeakReference(
          MakeVirtualSubscription(
              source.Subscribe(std::move(take_while_subscriber))),
          &sub_ref);

      if (take_while_ref) {
        take_while_ref->TakeSubscription(std::move(sub_ref));
      }

      return sub;
    });
  };
}

}  // namespace shk
