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
#include <rs/map.h>
#include <rs/pipe.h>
#include <rs/publisher.h>
#include <rs/subscriber.h>
#include <rs/subscription.h>
#include <rs/weak_reference.h>

namespace shk {
namespace detail {

/**
 * The Reduce class serves as a namespace with a shared set of template
 * parameters. It's there to avoid the verbosity of having to have several
 * classes with the same template parameters referring to each other.
 */
template <typename Accumulator, typename InnerSubscriber, typename Reducer>
class Reduce {
 public:
  class Emitter {
   public:
    Emitter(Accumulator &&accumulator, InnerSubscriber &&subscriber)
        : accumulator_(std::move(accumulator)),
          subscriber_(std::move(subscriber)) {}

    void Emit() {
      subscriber_.OnNext(std::move(accumulator_));
      subscriber_.OnComplete();
    }

   private:
    Accumulator accumulator_;
    InnerSubscriber subscriber_;
  };

  class ReduceSubscriber;

  class ReduceSubscription : public Subscription {
   public:
    ReduceSubscription() = default;

    explicit ReduceSubscription(AnySubscription &&inner_subscription)
        : inner_subscription_(std::move(inner_subscription)) {}

    void Request(ElementCount count) {
      if (emit_accumulated_value_) {
        emit_accumulated_value_->Emit();
        emit_accumulated_value_.reset();
      } else if (count > 0) {
        if (subscriber_) {
          subscriber_->requested_ = true;
        }
        inner_subscription_.Request(ElementCount::Unbounded());
      }
    }

    void Cancel() {
      inner_subscription_.Cancel();
    }

   private:
    friend class ReduceSubscriber;

    void TakeSubscriber(WeakReference<ReduceSubscriber> &&subscriber) {
      subscriber_ = std::move(subscriber);
    }

    // If the input stream finishes without any value emitted (this can happen
    // immediately or asynchronously),  then ReduceSubscriber gives the
    // accumulated value back to the ReduceSubscription, so that it can provide
    // a value when requested.
    std::unique_ptr<Emitter> emit_accumulated_value_;

    WeakReference<ReduceSubscriber> subscriber_;
    AnySubscription inner_subscription_;
  };

  class ReduceSubscriber : public Subscriber {
   public:
    ReduceSubscriber(
        Accumulator &&accumulator,
        InnerSubscriber &&subscriber,
        const Reducer &reducer)
        : accumulator_(std::move(accumulator)),
          subscriber_(std::move(subscriber)),
          reducer_(reducer) {}

    static void TakeSubscription(
        WeakReference<ReduceSubscriber> &&subscriber,
        WeakReference<ReduceSubscription> &&subscription) {
      subscriber->subscription_ = std::move(subscription);

      if (subscriber->complete_) {
        subscriber->AskSubscriptionToEmitAccumulatedValue();
      }

      subscriber->subscription_->TakeSubscriber(std::move(subscriber));
    }

    template <typename T, class = RequireRvalue<T>>
    void OnNext(T &&t) {
      if (failed_) {
        // Avoid calling the reducer more than necessary.
        return;
      }

      try {
        accumulator_ = reducer_(std::move(accumulator_), std::forward<T>(t));
      } catch (...) {
        failed_ = true;
        if (subscription_) {
          // If !subscription_, then the underlying subscription has been
          // destroyed and is by definition already cancelled so there is nothing
          // to do.
          subscription_->Cancel();
        }
        subscriber_.OnError(std::current_exception());
      }
    }

    void OnError(std::exception_ptr &&error) {
      subscriber_.OnError(std::move(error));
    }

    void OnComplete() {
      if (failed_) {
        // subscriber_.OnError has already been called; don't do anything else
        // with subscriber_.
      } else if (requested_) {
        subscriber_.OnNext(std::move(accumulator_));
        subscriber_.OnComplete();
      } else if (subscription_) {
        AskSubscriptionToEmitAccumulatedValue();
      } else {
        // This is reached if no value has been requested and if subscription_
        // is one of:
        //
        // 1) gone. In that case, no value will ever be requested so it's safe
        //    to do nothing here.
        // 2) not even given to TakeSubscription yet. In that case, a value
        //    might need to be emitted later.
        complete_ = true;
      }
    }

   private:
    friend class ReduceSubscription;

    void AskSubscriptionToEmitAccumulatedValue() {
      subscription_->emit_accumulated_value_.reset(
          new Emitter(std::move(accumulator_), std::move(subscriber_)));
    }

    bool complete_ = false;
    bool requested_ = false;

    bool failed_ = false;
    Accumulator accumulator_;
    InnerSubscriber subscriber_;
    Reducer reducer_;
    WeakReference<ReduceSubscription> subscription_;
  };
};

}  // namespace detail

/**
 * Like Reduce, but takes a function that returns the initial value instead of
 * the initial value directly. This is useful if the initial value is not
 * copyable.
 */
template <typename MakeInitial, typename ReducerT>
auto ReduceGet(MakeInitial &&make_initial, ReducerT &&reducer) {
  // Return an operator (it takes a Publisher and returns a Publisher)
  return [
      make_initial = std::forward<MakeInitial>(make_initial),
      reducer = std::forward<ReducerT>(reducer)](auto source) {
    // Return a Publisher
    return MakePublisher([make_initial, reducer, source = std::move(source)](
        auto &&subscriber) {
      using Accumulator = typename std::decay<decltype(make_initial())>::type;
      using InnerSubscriber = typename std::decay<decltype(subscriber)>::type;
      using Reducer = typename std::decay<ReducerT>::type;
      using ReduceSubscriberT = typename detail::Reduce<
          Accumulator, InnerSubscriber, Reducer>::ReduceSubscriber;
      using ReduceSubscriptionT = typename detail::Reduce<
          Accumulator, InnerSubscriber, Reducer>::ReduceSubscription;

      WeakReference<ReduceSubscriberT> reduce_ref;
      auto reduce_subscriber = WithWeakReference(
          ReduceSubscriberT(
              make_initial(),
              std::forward<decltype(subscriber)>(subscriber),
              reducer),
          &reduce_ref);

      WeakReference<ReduceSubscriptionT> sub_ref;
      auto sub = WithWeakReference(
          ReduceSubscriptionT(
              AnySubscription(source.Subscribe(std::move(reduce_subscriber)))),
          &sub_ref);

      if (reduce_ref) {
        ReduceSubscriberT::TakeSubscription(
            std::move(reduce_ref), std::move(sub_ref));
      }

      return sub;
    });
  };
}

/**
 * Like the reduce / fold operator in functional programming but over lists.
 *
 * Takes a stream of values and returns a stream of exactly one value.
 *
 * Initial must be copyable. If it isn't, consider using ReduceGet.
 */
template <typename Accumulator, typename Reducer>
auto Reduce(Accumulator &&initial, Reducer &&reducer) {
  return ReduceGet(
      [initial] { return initial; },
      std::forward<Reducer>(reducer));
}

/**
 * Like Reduce, but instead of taking an initial value, it requires that the
 * input stream has at least one value, and uses the first value of the stream
 * as the initial value. If the input stream is empty, it fails with an
 * std::out_of_range exception.
 *
 * This requires that the type of the input stream is convertible to the return
 * type of the reducer function (because if there is only one value, the reducer
 * is not invoked).
 *
 * This is used to implement the Last, Max and Min operators.
 */
template <typename Accumulator, typename Reducer>
auto ReduceWithoutInitial(Reducer &&reducer) {
  return BuildPipe(
    ReduceGet(
        [] { return std::unique_ptr<Accumulator>(); },
        [reducer = std::forward<Reducer>(reducer)](
            std::unique_ptr<Accumulator> &&accum, auto &&value) {
          if (accum) {
            return std::make_unique<Accumulator>(
                reducer(
                    std::move(*accum),
                    std::forward<decltype(value)>(value)));
          } else {
            return std::make_unique<Accumulator>(
                std::forward<decltype(value)>(value));
          }
        }),
    Map([](std::unique_ptr<Accumulator> &&value) {
      if (value) {
        return std::move(*value);
      } else {
        throw std::out_of_range(
            "ReduceWithoutInitial invoked with empty stream");
      }
    }));
}

}  // namespace shk
