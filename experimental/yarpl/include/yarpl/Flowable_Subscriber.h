// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <sstream>
#include <type_traits>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {
namespace flowable {

namespace {
template <typename T, typename Function>
class SubscriberWithOnNext : public reactivestreams_yarpl::Subscriber<T> {
 public:
  explicit SubscriberWithOnNext(Function&& function)
      : f_(std::move(function)) {}
  void onSubscribe(reactivestreams_yarpl::Subscription* s) override {
    s->request(INT64_MAX);
  }

  void onNext(const T& t) override {
    f_(t);
  }

  void onComplete() override {}

  void onError(const std::exception_ptr error) override {}

 private:
  Function f_;
};
}

class Subscribers {
 public:
  /**
   * Create a TestSubscriber that will subscribe upwards
   * with no flow control (max value) and store all values it receives.
   * @return
   */
  template <
      typename T,
      typename F,
      typename =
          typename std::enable_if<std::is_callable<F(T), void>::value>::type>
  static std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> create(
      F&& onNextFunc) {
    return std::make_unique<SubscriberWithOnNext<T, F>>(std::move(onNextFunc));
  }
};
}
}
