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

#include <deque>
#include <type_traits>

#include <rs/detail/optional.h>
#include <rs/element_count.h>
#include <rs/publisher.h>
#include <rs/subscriber.h>
#include <rs/subscription.h>
#include <rs/weak_reference.h>

namespace shk {
namespace detail {

template <typename InnerSubscriberType, typename Element>
class MergeSubscription : public Subscription {
  class MergeSubscriber : public Subscriber {
   public:
    MergeSubscriber(
        size_t idx,
        WeakReference<MergeSubscription> &&merge_subscription)
        : idx_(idx),
          merge_subscription_(std::move(merge_subscription)) {}

    void OnNext(Element &&elm) {
      if (merge_subscription_) {
        merge_subscription_->OnInnerSubscriptionNext(idx_, std::move(elm));
      }
    }

    void OnError(std::exception_ptr &&error) {
      if (merge_subscription_) {
        merge_subscription_->OnInnerSubscriptionError(std::move(error));
      }
    }

    void OnComplete() {
      if (merge_subscription_) {
        merge_subscription_->OnInnerSubscriptionComplete();
      }
    }

   private:
    size_t idx_;
    WeakReference<MergeSubscription> merge_subscription_;
  };

  struct MergeSubscriptionData {
    template <typename SubscriptionT>
    explicit MergeSubscriptionData(SubscriptionT &&subscription)
        : subscription(AnySubscription(
              std::forward<SubscriptionT>(subscription))) {}

    AnySubscription subscription;
    ElementCount outstanding;
  };

  template <size_t Idx, typename ...Publishers>
  struct MergeSubscriptionBuilder {
    template <typename SubscriptionReferee>
    static auto Subscribe(
        SubscriptionReferee &&merge_subscription,
        const std::tuple<Publishers...> &publishers) {
      WeakReference<MergeSubscription> subscription_ref;
      auto wrapped_merge_subscription = WithWeakReference(
          std::move(merge_subscription),
          &subscription_ref);

      if (!merge_subscription.finished_) {
        // Only subscribe if no previous Publisher has failed on Subscribe.
        wrapped_merge_subscription.subscriptions_.emplace_back(
            std::get<Idx>(publishers).Subscribe(
                MergeSubscriber(Idx, std::move(subscription_ref))));
      }

      return MergeSubscriptionBuilder<Idx + 1, Publishers...>::Subscribe(
          std::move(wrapped_merge_subscription),
          publishers);
    }
  };

  template <typename ...Publishers>
  struct MergeSubscriptionBuilder<sizeof...(Publishers), Publishers...> {
    template <typename SubscriptionReferee>
    static SubscriptionReferee Subscribe(
        SubscriptionReferee &&merge_subscription,
        const std::tuple<Publishers...> &publishers) {
      // Terminate the template recursion
      return std::move(merge_subscription);
    }
  };

 public:
  MergeSubscription() = default;

  explicit MergeSubscription(InnerSubscriberType &&inner_subscriber)
      : inner_subscriber_(std::move(inner_subscriber)) {}

  template <typename Subscriber, typename ...Publishers>
  static auto Subscribe(
      Subscriber &&subscriber,
      const std::tuple<Publishers...> &publishers) {
    MergeSubscription me(std::forward<Subscriber>(subscriber));

    me.remaining_subscriptions_ = sizeof...(Publishers);
    me.subscriptions_.reserve(sizeof...(Publishers));
    auto wrapped_me = MergeSubscriptionBuilder<0, Publishers...>::Subscribe(
        std::move(me), publishers);

    if (sizeof...(Publishers) == 0) {
      wrapped_me.SendOnComplete();
    }

    return wrapped_me;
  }

  void Request(ElementCount count) {
    if (!inner_subscriber_ || finished_) {
      return;
    }

    bool request_is_already_being_called =
        requested_elements_being_processed_ != 0;
    requested_elements_being_processed_ += count;
    if (request_is_already_being_called) {
      return;
    }

    outstanding_ += requested_elements_being_processed_;

    while (requested_elements_being_processed_ > 0 && !buffer_.empty()) {
      inner_subscriber_->OnNext(std::move(buffer_.front()));

      // Need to decrement this after calling OnNext, to ensure that re-entrant
      // Request calls always see that they are re-entrant.
      requested_elements_being_processed_--;
      buffer_.pop_front();
    }

    if (requested_elements_being_processed_ > 0) {
      for (auto &subscription_data : subscriptions_) {
        auto to_request =
            requested_elements_being_processed_ -
            subscription_data.outstanding;
        if (to_request > 0) {
          subscription_data.outstanding += to_request;
          subscription_data.subscription.Request(to_request);
        }
      }
    }

    // Need to reset requested_elements_being_processed_ in case it's Unbounded
    requested_elements_being_processed_ = 0;

    MaybeSendOnComplete();
  }

  void Cancel() {
    finished_ = true;
    for (auto &subscription_data : subscriptions_) {
      subscription_data.subscription.Cancel();
    }
  }

 private:
  void OnInnerSubscriptionNext(size_t idx, Element &&element) {
    if (finished_) {
      return;
    }

    if (idx >= subscriptions_.size()) {
      // This happens if the Publisher starts emitting values during the
      // Subscribe method.
      OnInnerSubscriptionError(std::make_exception_ptr(
          std::logic_error("Got value before Requesting anything")));
      return;
    }

    auto &outstanding = subscriptions_[idx].outstanding;
    if (outstanding <= 0) {
      OnInnerSubscriptionError(std::make_exception_ptr(
          std::logic_error("Got value that was not Request-ed")));
      return;
    }
    --outstanding;

    if (outstanding_ > 0) {
      --outstanding_;
      inner_subscriber_->OnNext(std::move(element));
    } else {
      buffer_.emplace_back(std::move(element));
    }
  }

  void OnInnerSubscriptionError(std::exception_ptr &&error) {
    if (!finished_) {
      Cancel();
      inner_subscriber_->OnError(std::move(error));
    }
  }

  void OnInnerSubscriptionComplete() {
    remaining_subscriptions_--;
    MaybeSendOnComplete();
  }

  void MaybeSendOnComplete() {
    if (!finished_ && remaining_subscriptions_ == 0 && buffer_.empty()) {
      SendOnComplete();
    }
  }

  void SendOnComplete() {
    finished_ = true;
    inner_subscriber_->OnComplete();
  }

  std::deque<Element> buffer_;
  // The number of elements that have been requested from this stream that have
  // not yet been delivered.
  ElementCount outstanding_;

  // The number of elements that have been asked to be processed by calling
  // Request that it has not yet had the time to request. This is used to make
  // sure that in the cases where Request calls OnNext and OnNext calls Request,
  // it does not call OnNext again.
  ElementCount requested_elements_being_processed_;

  size_t remaining_subscriptions_ = 0;
  bool finished_ = false;
  detail::Optional<InnerSubscriberType> inner_subscriber_;
  // With each subscription, also keep track of how many requested elements are
  // outstanding. This is used to make sure that for any given subscription, we
  // don't have more elements Requested than what has been requested for the
  // merged stream. This is necessary to stay within the documented upper bound
  // for buffer size.
  //
  // This is empty until the call to Subscribe and may be empty after
  // cancellation.
  std::vector<MergeSubscriptionData> subscriptions_;
};

}  // namespace detail

/**
 * Merge combines multiple streams into one. All elements of the incoming
 * streams are emitted in the combined stream.
 *
 * In order to not violate the backpressure invariants, Merge may need to buffer
 * up to [number of input streams - 1] * [outstanding requested elements]
 * elements. If an unbounded number of elements are requested, no buffering is
 * performed.
 */
template <typename Element, typename ...Publishers>
auto Merge(Publishers &&...publishers) {
  return MakePublisher([publishers = std::make_tuple(
      std::forward<Publishers>(publishers)...)](auto &&subscriber) {
    using MergeSubscriptionT = detail::MergeSubscription<
        typename std::decay<decltype(subscriber)>::type,
        Element>;

    return MergeSubscriptionT::Subscribe(
        std::forward<decltype(subscriber)>(subscriber),
        publishers);
  });
}

}  // namespace shk
