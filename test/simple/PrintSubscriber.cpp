// Copyright 2004-present Facebook. All Rights Reserved.

#include "PrintSubscriber.h"
#include <folly/Memory.h>
#include <folly/io/IOBufQueue.h>
#include <glog/logging.h>

namespace reactivesocket {

PrintSubscriber::~PrintSubscriber() {
  LOG(INFO) << "~PrintSubscriber " << this;
}

void PrintSubscriber::onSubscribe(std::shared_ptr<Subscription> subscription) {
  LOG(INFO) << "PrintSubscriber " << this << " onSubscribe";
  subscription->request(std::numeric_limits<int32_t>::max());
}

void PrintSubscriber::onNext(Payload element) {
  LOG(INFO) << "PrintSubscriber " << this << " onNext " << element;
}

void PrintSubscriber::onComplete() {
  LOG(INFO) << "PrintSubscriber " << this << " onComplete";
}

void PrintSubscriber::onError(folly::exception_wrapper ex) {
  LOG(INFO) << "PrintSubscriber " << this << " onError " << ex.what();
}
}
