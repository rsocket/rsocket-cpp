// Copyright 2004-present Facebook. All Rights Reserved.

#include "tck-test/SingleSubscriber.h"

#include <thread>

#include <folly/Format.h>

using namespace folly;

namespace rsocket {
namespace tck {

void SingleSubscriber::request(int n) {
  LOG(INFO) << "... requesting " << n << ". No request() for Single.";
}

void SingleSubscriber::cancel() {
  LOG(INFO) << "... canceling ";
  canceled_ = true;
  if (auto subscription = std::move(subscription_)) {
    subscription->cancel();
  }
}

void SingleSubscriber::onSubscribe(
    yarpl::Reference<yarpl::single::SingleSubscription> subscription) noexcept {
  VLOG(4) << "OnSubscribe in SingleSubscriber";
  subscription_ = subscription;
}

void SingleSubscriber::onSuccess(Payload element) noexcept {
  LOG(INFO) << "... received onSuccess from Publisher: " << element;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    std::string data =
        element.data ? element.data->moveToFbString().toStdString() : "";
    std::string metadata = element.metadata
        ? element.metadata->moveToFbString().toStdString()
        : "";
    values_.push_back(std::make_pair(data, metadata));
    ++valuesCount_;
  }
  valuesCV_.notify_one();
  {
    std::unique_lock<std::mutex> lock(mutex_);
    completed_ = true;
  }
  terminatedCV_.notify_one();
}

void SingleSubscriber::onError(std::exception_ptr ex) noexcept {
  LOG(INFO) << "... received onError from Publisher";
  {
    std::unique_lock<std::mutex> lock(mutex_);
    errors_.push_back(std::move(ex));
    errored_ = true;
  }
  terminatedCV_.notify_one();
}

} // tck
} // reactivesocket
