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

#include <memory>

#include <rs/element_count.h>
#include <rs/publisher.h>
#include <rs/subscriber.h>
#include <rs/subscription.h>
#include <rs/weak_reference.h>

namespace shk {
namespace detail {

template <
    typename InnerSubscriberType,
    typename Callback,
    typename ErrorSubscription>
class Catch {
 public:
  class CatchSubscriber;

  class CatchSubscription : public Subscription {
   public:
    CatchSubscription() = default;

    explicit CatchSubscription(WeakReference<CatchSubscriber> &&subscriber)
        : subscriber_(std::move(subscriber)) {}

    void Request(ElementCount count) {
      if (subscriber_) {
        subscriber_->requested_ += count;
      }

      catch_subscription_.Request(count);
      inner_subscription_.Request(count);
    }

    void Cancel() {
      if (subscriber_) {
        subscriber_->cancelled_ = true;
      }

      inner_subscription_.Cancel();
      catch_subscription_.Cancel();
    }

   private:
    friend class CatchSubscriber;

    // The subscription to the non-catch-clause Publisher. Set only once, to
    // avoid the risk of destroying a Subscription object that is this of a
    // current stack frame, causing memory corruption.
    AnySubscription inner_subscription_;
    ErrorSubscription catch_subscription_;
    WeakReference<CatchSubscriber> subscriber_;
  };

  class CatchSubscriber : public Subscriber {
   public:
    CatchSubscriber(
        InnerSubscriberType &&inner_subscriber,
        const Callback &callback)
        : inner_subscriber_(std::move(inner_subscriber)),
          callback_(callback) {}

    template <typename Publisher>
    static void Subscribe(
        WeakReferee<WeakReferee<CatchSubscriber>> &&me,
        WeakReference<CatchSubscriber> &&me_ref,
        WeakReference<CatchSubscription> &&subscription,
        Publisher &&publisher) {
      me.subscription_ = std::move(subscription);
      auto sub = publisher.Subscribe(std::move(me));
      if (me_ref && me_ref->subscription_) {
        me_ref->subscription_->inner_subscription_ =
            AnySubscription(std::move(sub));
      }
    }

    template <typename T, class = RequireRvalue<T>>
    void OnNext(T &&t) {
      --requested_;
      if (requested_ < 0) {
        cancelled_ = true;
        // TODO(peck): This should cancel the underlying subscriptions too.
        inner_subscriber_.OnError(std::make_exception_ptr(
            std::logic_error("Got value that was not Request-ed")));
      } else {
        inner_subscriber_.OnNext(std::forward<T>(t));
      }
    }

    void OnError(std::exception_ptr &&error) {
      if (cancelled_) {
        // Do nothing
      } else {
        has_failed_ = true;
        auto catch_publisher = callback_(std::move(error));
        static_assert(
            IsPublisher<decltype(catch_publisher)>,
            "Catch callback must return a Publisher");

        auto sub = catch_publisher.Subscribe(std::move(inner_subscriber_));
        sub.Request(requested_);
        if (subscription_) {
          subscription_->catch_subscription_ = std::move(sub);
        }
      }
    }

    void OnComplete() {
      if (!cancelled_) {
        inner_subscriber_.OnComplete();
      }
    }

   private:
    friend class CatchSubscription;

    // The number of elements that have been requested but not yet emitted.
    ElementCount requested_;
    // If the subscription has been cancelled. This is important to keep track
    // of because a cancelled subscription may fail, and in that case we don't
    // want to subscribe to the catch Publisher since that would undo the
    // cancellation.
    bool cancelled_ = false;

    WeakReference<CatchSubscription> subscription_;
    InnerSubscriberType inner_subscriber_;
    Callback callback_;
    // The number of elements that have been requested but not yet emitted.
    bool has_failed_ = false;
  };
};

}  // namespace detail

/**
 * Catch is an asynchronous version of a try/catch statement. It makes an
 * operator that takes a Publisher and returns a Publisher that behaves exactly
 * the same, except if it ends with an error. If so, callback is called and the
 * stream continues with the Publisher that Callback returned.
 */
template <typename Callback>
auto Catch(Callback &&callback) {
  // Return an operator (it takes a Publisher and returns a Publisher)
  return [callback = std::forward<Callback>(callback)](auto source) {
    // Return a Publisher
    return MakePublisher([callback, source = std::move(source)](
        auto &&subscriber) {
      using ErrorSubscription =
          decltype(callback(std::declval<std::exception_ptr>())
              .Subscribe(std::move(subscriber)));
      using CatchT = detail::Catch<
          typename std::decay<decltype(subscriber)>::type,
          typename std::decay<Callback>::type,
          ErrorSubscription>;
      using CatchSubscriberT = typename CatchT::CatchSubscriber;
      using CatchSubscriptionT = typename CatchT::CatchSubscription;

      WeakReference<CatchSubscriberT> subscriber_ref;
      WeakReference<CatchSubscriberT> subscriber_ref_2;
      auto catch_subscriber = WithWeakReference(
          CatchSubscriberT(
              std::forward<decltype(subscriber)>(subscriber),
              callback),
          &subscriber_ref,
          &subscriber_ref_2);

      WeakReference<CatchSubscriptionT> sub_ref;
      auto subscription = WithWeakReference(
          CatchSubscriptionT(std::move(subscriber_ref)),
          &sub_ref);

      CatchSubscriberT::Subscribe(
          std::move(catch_subscriber),
          std::move(subscriber_ref_2),
          std::move(sub_ref),
          source);

      return subscription;
    });
  };
}

}  // namespace shk
