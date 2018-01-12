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
class FilterSubscriber : public Subscriber {
 public:
  FilterSubscriber(
      InnerSubscriberType &&inner_subscriber,
      const Predicate &predicate)
      : inner_subscriber_(std::move(inner_subscriber)),
        predicate_(predicate) {}

  void TakeSubscription(WeakReference<PureVirtualSubscription> &&subscription) {
    subscription_ = std::move(subscription);
  }

  template <typename T, class = RequireRvalue<T>>
  void OnNext(T &&t) {
    if (failed_) {
      return;
    }

    bool predicate_match = false;
    try {
      predicate_match = predicate_(static_cast<const T &>(t));
    } catch (...) {
      if (subscription_) {
        // If !subscription_, then the underlying subscription has been
        // destroyed and is by definition already cancelled so there is nothing
        // to do.
        subscription_->Cancel();
      }
      failed_ = true;
      inner_subscriber_.OnError(std::current_exception());
    }
    if (!failed_) { // If predicate_ threw, failed_ could be true here
      if (predicate_match) {
        inner_subscriber_.OnNext(std::forward<T>(t));
      } else {
        if (subscription_) {
          // If !subscription_, then the underlying subscription is cancelled.
          subscription_->Request(ElementCount(1));
        }
      }
    }
  }

  void OnError(std::exception_ptr &&error) {
    if (!failed_) {
      inner_subscriber_.OnError(std::move(error));
    }
  }

  void OnComplete() {
    if (!failed_) {
      inner_subscriber_.OnComplete();
    }
  }

 private:
  bool failed_ = false;
  InnerSubscriberType inner_subscriber_;
  WeakReference<PureVirtualSubscription> subscription_;
  Predicate predicate_;
};

}  // namespace detail

/**
 * Filter is like the functional filter operator that operates on a Publisher.
 */
template <typename Predicate>
auto Filter(Predicate &&predicate) {
  // Return an operator (it takes a Publisher and returns a Publisher)
  return [predicate = std::forward<Predicate>(predicate)](auto source) {
    // Return a Publisher
    return MakePublisher([predicate, source = std::move(source)](
        auto &&subscriber) {
      using FilterSubscriberT = detail::FilterSubscriber<
          typename std::decay<decltype(subscriber)>::type,
          typename std::decay<Predicate>::type>;

      WeakReference<FilterSubscriberT> filter_ref;
      auto filter_subscriber = WithWeakReference(
          FilterSubscriberT(
              std::forward<decltype(subscriber)>(subscriber),
              predicate),
          &filter_ref);

      WeakReference<PureVirtualSubscription> sub_ref;
      auto sub = WithWeakReference(
          MakeVirtualSubscription(
              source.Subscribe(std::move(filter_subscriber))),
          &sub_ref);

      if (filter_ref) {
        filter_ref->TakeSubscription(std::move(sub_ref));
      }

      return sub;
    });
  };
}

}  // namespace shk
