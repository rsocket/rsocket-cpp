// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/flowable/Flowable.h"

namespace yarpl {
namespace flowable {
namespace details {

template <typename T, typename FlowableFactory>
class DeferFlowable : public Flowable<T> {
 public:
  DeferFlowable(FlowableFactory factory) : factory_(std::move(factory)) {}

  virtual void subscribe(std::shared_ptr<Subscriber<T>> subscriber) {
    std::shared_ptr<Flowable<T>> flowable;
    try {
      flowable = factory_();
    } catch (const std::exception& ex) {
      flowable = Flowable<T>::error(ex, std::current_exception());
    }
    flowable->subscribe(std::move(subscriber));
  }

 private:
  FlowableFactory factory_;
};

} // namespace details
} // namespace flowable
} // namespace yarpl
