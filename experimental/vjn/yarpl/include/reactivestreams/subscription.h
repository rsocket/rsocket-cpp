#pragma once

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include "subscriber.h"

namespace reactivestreams {

template <typename T>
class Subscription {
 public:
  virtual ~Subscription() = default;
  virtual void request(long n) = 0;
  virtual void cancel() = 0;
};

template <typename T, typename Next>
class DeletingSubscription : public Subscription<T> {
 public:
  DeletingSubscription(std::shared_ptr<Next> function,
                       std::unique_ptr<Subscriber<T>> subscriber)
      : function_(function), subscriber_(std::move(subscriber)) {}

  void start() {
    subscriber_->on_subscribe(this);

    try {
      (*function_)(*subscriber_);
      subscriber_->on_complete();
    } catch (...) {
      subscriber_->on_error(std::current_exception());
    }

    delete this;
  }

  void request(long) {}
  void cancel() {}

 private:
  std::shared_ptr<Next> function_;
  std::unique_ptr<Subscriber<T>> subscriber_;
};

}  // reactivestreams
