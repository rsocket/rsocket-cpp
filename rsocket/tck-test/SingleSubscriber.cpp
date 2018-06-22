// Copyright (c) Facebook, Inc. and its affiliates.
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

#include "rsocket/tck-test/SingleSubscriber.h"

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
    std::shared_ptr<yarpl::single::SingleSubscription> subscription) noexcept {
  VLOG(4) << "OnSubscribe in SingleSubscriber";
  subscription_ = subscription;
}

void SingleSubscriber::onSuccess(Payload element) noexcept {
  LOG(INFO) << "... received onSuccess from Publisher: " << element;
  {
    const std::unique_lock<std::mutex> lock(mutex_);
    const std::string data =
        element.data ? element.data->moveToFbString().toStdString() : "";
    const std::string metadata = element.metadata
        ? element.metadata->moveToFbString().toStdString()
        : "";
    values_.push_back(std::make_pair(data, metadata));
    ++valuesCount_;
  }
  valuesCV_.notify_one();
  {
    const std::unique_lock<std::mutex> lock(mutex_);
    completed_ = true;
  }
  terminatedCV_.notify_one();
}

void SingleSubscriber::onError(folly::exception_wrapper ex) noexcept {
  LOG(INFO) << "... received onError from Publisher";
  {
    const std::unique_lock<std::mutex> lock(mutex_);
    errors_.push_back(std::move(ex));
    errored_ = true;
  }
  terminatedCV_.notify_one();
}

} // namespace tck
} // namespace rsocket
