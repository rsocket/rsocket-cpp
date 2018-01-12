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
#include <rs/subscriber.h>
#include <rs/subscription.h>
#include <rs/weak_reference.h>

namespace shk {
namespace detail {

/**
 * The ConcatMap class serves as a namespace with a shared set of template
 * parameters. It's there to avoid the verbosity of having to have several
 * classes with the same template parameters referring to each other.
 */
template <
    typename InnerSubscriberType,
    typename Mapper,
    typename SourcePublisher>
class ConcatMap {
 public:
  class ConcatMapValuesSubscriber;
  class ConcatMapPublishersSubscriber;

  class ConcatMapSubscription : public Subscription {
   public:
    ConcatMapSubscription() = default;

    explicit ConcatMapSubscription(InnerSubscriberType &&inner_subscriber)
        : inner_subscriber_(std::move(inner_subscriber)) {}

    void Request(ElementCount count) {
      to_be_requested_ += count;
      if (subscription_) {
        do {
          ElementCount to_be_requested = to_be_requested_;
          // Reset to_be_requested_ before the call to subscription->Request
          // because that might bump that number again.
          to_be_requested_ = 0;
          // Increase requested_ before the call to Request because otherwise
          // the backpressure violation logic might erroneously trigger.
          requested_ += to_be_requested;
          // There is a possibility that this call to Request will call
          // OnValuesComplete, which might request a new Publisher, which might
          // invoke OnPublishersNext, which will reset subscription_, which will
          // destroy the current subscription_, which would cause a this pointer
          // that is currently on the stack to be destroyed, causing all kinds of
          // brokenness.
          //
          // To avoid this, let's steal subscription_ and keep it on the stack to
          // guarantee that it won't get destroyed too early.
          auto subscription = std::move(subscription_);
          subscription->Request(to_be_requested);
          // Now we need to reset the subscription_ pointer, but if subscription_
          // has been written it means that the Subscription that we called has
          // been completed and should be left to die.
          if (!subscription_) {
            subscription_ = std::move(subscription);
          }
        } while (to_be_requested_ != 0);
      }
    }

    void Cancel() {
      if (subscription_) {
        subscription_->Cancel();
      }
      publishers_subscription_.Cancel();
    }

    void SubscribeForPublishers(
        const Mapper &mapper,
        const SourcePublisher &source,
        WeakReference<ConcatMapSubscription> &&self_ref_a,
        WeakReference<ConcatMapSubscription> &&self_ref_b) {
      self_ref_ = std::move(self_ref_a);
      publishers_subscription_ = AnySubscription(source.Subscribe(
          ConcatMapPublishersSubscriber(
              mapper,
              std::move(self_ref_b))));
      publishers_subscription_.Request(ElementCount(1));
    }

   private:
    friend class ConcatMapValuesSubscriber;
    friend class ConcatMapPublishersSubscriber;

    template <typename T>
    void OnValuesNext(T &&t) {
      if (failed_) {
        // This avoids calling OnError multiple times on backpressure
        // violations.
        return;
      }

      if (requested_ == 0) {
        OnError(std::make_exception_ptr(
            std::logic_error("Got value that was not Request-ed")));
        return;
      }
      --requested_;
      inner_subscriber_->OnNext(std::forward<T>(t));
    }

    void OnError(std::exception_ptr &&error) {
      failed_ = true;
      Cancel();
      inner_subscriber_->OnError(std::move(error));
    }

    void OnValuesComplete(WeakReference<ConcatMapSubscription> &&self_ref) {
      if (failed_) {
        return;
      }

      if (publishers_complete_) {
        inner_subscriber_->OnComplete();
      } else {
        self_ref_ = std::move(self_ref);
        publishers_subscription_.Request(ElementCount(1));
      }
    }

    template <typename T>
    void OnPublishersNext(T &&publisher) {
      if (failed_) {
        return;
      }

      if (!self_ref_) {
        OnError(std::make_exception_ptr(
            std::logic_error("Got value that was not Request-ed")));
        return;
      }
      subscription_ = MakeVirtualSubscriptionPtr(publisher.Subscribe(
              ConcatMapValuesSubscriber(std::move(self_ref_))));
      subscription_->Request(requested_);
    }

    void OnPublishersComplete() {
      if (failed_) {
        return;
      }

      publishers_complete_ = true;
      if (self_ref_) {
        // This happens when the Publishers subscription completes when there is
        // no current values Publisher.
        inner_subscriber_->OnComplete();
      }
    }

    bool failed_ = false;
    bool publishers_complete_ = false;
    // The number of elements that have been Requested and not yet delivered.
    ElementCount requested_;
    // The sum of all ElementCounts that have been sent to Request but not yet
    // been requested from subscription_.
    ElementCount to_be_requested_;
    // The subscription to the current values Publisher. This is re-set whenever
    // a new values Publisher is received. But beware! There is a risk that the
    // time that this class wants to set subscription_ is when the subscription_
    // object itself is this of current stack frames. In those cases, it is not
    // safe to set (and consequently destroy the previous) subscription_.
    std::unique_ptr<PureVirtualSubscription> subscription_;
    // During times when there is no current Publisher that is being flattened,
    // the ConcatMapSubscription holds a WeakReference to itself.
    WeakReference<ConcatMapSubscription> self_ref_;
    // Set once and then never set again.
    AnySubscription publishers_subscription_;
    // Set once (at construction) and then never set again.
    detail::Optional<InnerSubscriberType> inner_subscriber_;
  };

  class ConcatMapPublishersSubscriber : public Subscriber {
   public:
    ConcatMapPublishersSubscriber(
        const Mapper &mapper,
        WeakReference<ConcatMapSubscription> &&subscription)
        : mapper_(mapper),
          subscription_(std::move(subscription)) {}

    template <typename T>
    void OnNext(T &&t) {
      if (!subscription_) {
        return;
      }

      try {
        auto publisher = mapper_(std::forward<T>(t));
        static_assert(
            IsPublisher<decltype(publisher)>,
            "ConcatMap mapper function must return Publisher");

        if (/*TODO(peck):Test this*/subscription_) {
          // It is possible that calling the mapper cancels the subscription,
          // which might make subscription_ invalid. So it's good to double
          // check.
          subscription_->OnPublishersNext(std::move(publisher));
        }
      } catch (...) {
        OnError(std::current_exception());
      }
    }

    void OnError(std::exception_ptr &&error) {
      if (subscription_) {
        subscription_->OnError(std::move(error));
      }
    }

    void OnComplete() {
      if (subscription_) {
        subscription_->OnPublishersComplete();
      }
    }

   private:
    Mapper mapper_;
    WeakReference<ConcatMapSubscription> subscription_;
  };

  class ConcatMapValuesSubscriber : public Subscriber {
   public:
    ConcatMapValuesSubscriber(
        WeakReference<ConcatMapSubscription> &&subscription)
        : subscription_(std::move(subscription)) {}

    template <typename T>
    void OnNext(T &&t) {
      if (subscription_) {
        subscription_->OnValuesNext(std::forward<T>(t));
      }
    }

    void OnError(std::exception_ptr &&error) {
      if (subscription_) {
        subscription_->OnError(std::move(error));
      }
    }

    void OnComplete() {
      if (subscription_) {
        subscription_->OnValuesComplete(std::move(subscription_));
      }
    }

   private:
    friend class ConcatMapSubscription;

    WeakReference<ConcatMapSubscription> subscription_;
  };
};

}  // namespace detail

/**
 * ConcatMap is like the functional flatMap operator that operates on a
 * Publisher: The mapper function returns a Publisher, which may emit zero or
 * more values. All of the Publishers returned by the mapper are concatenated,
 * or "flattened", into a single Publisher.
 */
template <typename MapperT>
auto ConcatMap(MapperT &&mapper) {
  // Return an operator (it takes a Publisher and returns a Publisher)
  return [mapper = std::forward<MapperT>(mapper)](auto source) {
    // Return a Publisher
    return MakePublisher([mapper, source = std::move(source)](
        auto &&subscriber) {
      using InnerSubscriberType =
          typename std::decay<decltype(subscriber)>::type;
      using Mapper = typename std::decay<MapperT>::type;

      using ConcatMapT = detail::ConcatMap<
          InnerSubscriberType, Mapper, decltype(source)>;

      using ConcatMapSubscriptionT =
          typename ConcatMapT::ConcatMapSubscription;

      WeakReference<ConcatMapSubscriptionT> sub_ref_a;
      WeakReference<ConcatMapSubscriptionT> sub_ref_b;
      auto sub = WithWeakReference(
          ConcatMapSubscriptionT(
              std::forward<decltype(subscriber)>(subscriber)),
          &sub_ref_b,
          &sub_ref_a);

      sub.SubscribeForPublishers(
          mapper,
          source,
          std::move(sub_ref_a),
          std::move(sub_ref_b));

      return sub;
    });
  };
}

}  // namespace shk
