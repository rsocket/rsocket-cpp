// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/flowable/Subscriber.h"

#include <stdexcept>

namespace yarpl {
namespace flowable {

/**
 * A Subscriber that always cancels the subscription passed to it.
 */
template <typename T>
class CancelingSubscriber final : public Subscriber<T> {
 public:
  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> sub) override {
    sub->cancel();
  }

  void onNext(T) override {
    throw std::logic_error{"CancelingSubscriber::onNext() can never be called"};
  }
};
}
}
