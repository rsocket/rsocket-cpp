// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include "yarpl/Flowable_TestSubscriber.h"
#include "yarpl/flowable/sources/Flowable_RangeSubscription.h"

#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/utils/type_traits.h"

#include "yarpl/flowable/operators/Flowable_Map.h"
#include "yarpl/flowable/operators/Flowable_SubscribeOn.h"
#include "yarpl/flowable/operators/Flowable_Take.h"

namespace yarpl {
namespace flowable {

template <typename T>
class FlowableB : public reactivestreams_yarpl::Publisher<T> {
  // using reactivestream_yarpl to not conflict with the other reactivestreams
  using Subscriber = reactivestreams_yarpl::Subscriber<T>;
  using Subscription = reactivestreams_yarpl::Subscription;

 public:
  explicit FlowableB(std::unique_ptr<reactivestreams_yarpl::Publisher<T>>&& p)
      : publisher_(std::move(p)){};
    ~FlowableB() {
        std::cout << "FlowableB being destroyed" << std::endl;
    }
  FlowableB(FlowableB&&) = default;
  FlowableB(const FlowableB&) = delete;
  FlowableB& operator=(FlowableB&&) = default;
  FlowableB& operator=(const FlowableB&) = delete;

  void subscribe(std::unique_ptr<Subscriber> subscriber) override {
    publisher_->subscribe(std::move(subscriber));
  }

  template <
      typename R,
      typename F,
      typename = typename std::enable_if<std::is_callable<
          F(std::unique_ptr<reactivestreams_yarpl::Subscriber<R>>),
          std::unique_ptr<reactivestreams_yarpl::Subscriber<T>>>::value>::type>
  FlowableB<R> lift(F&& onSubscribeLift) {
    class LiftedPublisher : public reactivestreams_yarpl::Publisher<R> {
     public:
      LiftedPublisher(
          std::unique_ptr<reactivestreams_yarpl::Publisher<T>>&& upstream,
          F&& onSubscribeLift)
          : upstream_(std::move(upstream)),
            onSubscribeLift_(std::move(onSubscribeLift)) {}
        ~LiftedPublisher() {
            std::cout << "Lifted Publisher destroyed!!!" << std::endl;
        }
      void subscribe(
          std::unique_ptr<reactivestreams_yarpl::Subscriber<R>> s) override {
        upstream_->subscribe(onSubscribeLift_(std::move(s)));
      }

     private:
      std::unique_ptr<reactivestreams_yarpl::Publisher<T>> upstream_;
      F&& onSubscribeLift_;
    };

    auto newP = std::make_unique<LiftedPublisher>(
        std::move(publisher_), std::move(onSubscribeLift));

    return FlowableB<R>(std::move(newP));
  }

  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::
          type>
  FlowableB<typename std::result_of<F(T)>::type> map(F&& function) {
    return lift<typename std::result_of<F(T)>::type>(
        yarpl::operators::
            FlowableMapOperator<T, typename std::result_of<F(T)>::type, F>(
                std::forward<F>(function)));
  }

  FlowableB<T> take(int64_t toTake) {
    return lift<T>(yarpl::operators::FlowableTakeOperator<T>(toTake));
  }

  FlowableB<T> subscribeOn(yarpl::Scheduler& scheduler) {
    return lift<T>(yarpl::operators::FlowableSubscribeOnOperator<T>(scheduler));
  }

 private:
  std::unique_ptr<reactivestreams_yarpl::Publisher<T>> publisher_;
};

class FlowablesB {
 public:
  FlowablesB() = default;
  FlowablesB(FlowablesB&&) = delete;
  FlowablesB(const FlowablesB&) = delete;
  FlowablesB& operator=(FlowablesB&&) = delete;
  FlowablesB& operator=(const FlowablesB&) = delete;

  static FlowableB<long> range(long start, long count) {
    class RangePublisher : public reactivestreams_yarpl::Publisher<long> {
     public:
        RangePublisher(long start, long count) : start_(start), count_(count) {}
        ~RangePublisher() {
            std::cout << "RangePublisher being DESTROYED " << std::endl;
        }
      void subscribe(
          std::unique_ptr<reactivestreams_yarpl::Subscriber<long>> s) override {
        auto rs = new yarpl::flowable::sources::RangeSubscription(
            start_, count_, std::move(s));
        rs->start();
      }

     private:
      long start_;
      long count_;
    };
    return FlowableB<long>(std::make_unique<RangePublisher>(start, count));
  };

  template <typename T>
  static auto fromPublisher(
      std::unique_ptr<reactivestreams_yarpl::Publisher<T>> p) {
    return FlowableB<T>(std::move(p));
  }
};

} // flowable
} // yarpl
