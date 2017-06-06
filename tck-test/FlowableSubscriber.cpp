#include "tck-test/FlowableSubscriber.h"

#include <thread>

#include <folly/Format.h>

using namespace folly;

namespace rsocket {
namespace tck {

FlowableSubscriber::FlowableSubscriber(int initialRequestN)
    : initialRequestN_(initialRequestN) {}

void FlowableSubscriber::request(int n) {
  LOG(INFO) << "... requesting " << n;
  while (!subscription_) {
    ;
  }
  subscription_->request(n);
}

void FlowableSubscriber::cancel() {
  LOG(INFO) << "... canceling ";
  canceled_ = true;
  if (auto subscription = std::move(subscription_)) {
    subscription->cancel();
  }
}

void FlowableSubscriber::onSubscribe(
    yarpl::Reference<yarpl::flowable::Subscription> subscription) noexcept {
  VLOG(4) << "OnSubscribe in FlowableSubscriber";
  subscription_ = subscription;
  if (initialRequestN_ > 0) {
    subscription_->request(initialRequestN_);
  }
}

void FlowableSubscriber::onNext(Payload element) noexcept {
  LOG(INFO) << "... received onNext from Publisher: " << element;
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
}

void FlowableSubscriber::onComplete() noexcept {
  LOG(INFO) << "... received onComplete from Publisher";
  {
    std::unique_lock<std::mutex> lock(mutex_);
    completed_ = true;
  }

  terminatedCV_.notify_one();
}

void FlowableSubscriber::onError(std::exception_ptr ex) noexcept {
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
