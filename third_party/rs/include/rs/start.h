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

#include <rs/detail/optional.h>
#include <rs/element_count.h>
#include <rs/publisher.h>
#include <rs/subscription.h>

namespace shk {
namespace detail {

template <size_t TryIndex, typename ...CreateValue>
struct InvokeOnNext {
  template <typename Subscriber>
  static void Invoke(
      size_t index,
      Subscriber &subscriber,
      std::tuple<CreateValue...> &create_values) {
    if (index + 1 == TryIndex) {
      subscriber.OnNext(std::get<TryIndex - 1>(create_values)());
    } else {
      InvokeOnNext<TryIndex - 1, CreateValue...>::Invoke(
          index, subscriber, create_values);
    }
  }
};

template <typename ...CreateValue>
struct InvokeOnNext<0, CreateValue...> {
  template <typename Subscriber>
  static void Invoke(
      size_t index,
      Subscriber &subscriber,
      std::tuple<CreateValue...> &create_values) {
    // Not found
  }
};

template <typename Subscriber, typename ...CreateValue>
class StartSubscription : public Subscription {
 public:
  StartSubscription() = default;

  template <typename SubscriberT>
  StartSubscription(
      const std::tuple<CreateValue...> &create_values,
      SubscriberT &&subscriber)
      : create_values_(std::tuple<CreateValue...>(std::move(create_values))),
        subscriber_(
            new Subscriber(std::forward<SubscriberT>(subscriber))) {
    if (sizeof...(CreateValue) == 0) {
      subscriber_->OnComplete();
    }
  }

  void Request(ElementCount count) {
    if (!subscriber_) {
      // This is a default constructed (ie cancelled) subscription
      return;
    }

    bool has_outstanding_request_count = outstanding_request_count_ != 0;
    outstanding_request_count_ += count;
    if (has_outstanding_request_count) {
      // Farther up in the stack, Request is already being called. No need
      // to do anything here.
      return;
    }

    while (at_ < sizeof...(CreateValue) && outstanding_request_count_ != 0) {
      InvokeOnNext<sizeof...(CreateValue), CreateValue...>::Invoke(
          at_++, *subscriber_, *create_values_);

      if (at_ == sizeof...(CreateValue)) {
        subscriber_->OnComplete();
      }

      // Need to decrement this after calling OnNext/OnComplete, to ensure that
      // re-entrant Request calls always see that they are re-entrant.
      --outstanding_request_count_;
    }
  }

  void Cancel() {
    at_ = sizeof...(CreateValue);
  }

 private:
  detail::Optional<std::tuple<CreateValue...>> create_values_;
  // TODO(peck): It would be nice to make this an optional instead of
  // unique_ptr; there is no need for this heap allocation.
  std::unique_ptr<Subscriber> subscriber_;
  size_t at_ = 0;
  ElementCount outstanding_request_count_ = ElementCount(0);
};


}  // namespace detail

template <typename ...CreateValue>
auto Start(CreateValue &&...create_value) {
  return MakePublisher([
      create_values = std::make_tuple(
          std::forward<CreateValue>(create_value)...)](
              auto &&subscriber) {
    return detail::StartSubscription<
        typename std::decay<decltype(subscriber)>::type,
        typename std::decay<CreateValue>::type...>(
            create_values,
            std::forward<decltype(subscriber)>(subscriber));
  });
}

}  // namespace shk
