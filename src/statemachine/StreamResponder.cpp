// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/statemachine/StreamResponder.h"
#include <folly/ExceptionString.h>

namespace rsocket {

using namespace yarpl;
using namespace yarpl::flowable;

void StreamResponder::onSubscribe(
    Reference<yarpl::flowable::Subscription> subscription) noexcept {
  publisherSubscribe(std::move(subscription));
}

void StreamResponder::onNext(Payload response) noexcept {
  checkPublisherOnNext();
  writePayload(std::move(response), false);
}

void StreamResponder::onComplete() noexcept {
  publisherComplete();
  completeStream();
  closeStream(StreamCompletionSignal::COMPLETE);
}

void StreamResponder::onError(const std::exception_ptr ex) noexcept {
  publisherComplete();
  applicationError(folly::exceptionStr(ex).toStdString());
  closeStream(StreamCompletionSignal::COMPLETE);
}

//void StreamResponder::pauseStream(RequestHandler& requestHandler) {
//  pausePublisherStream(requestHandler);
//}
//
//void StreamResponder::resumeStream(RequestHandler& requestHandler) {
//  resumePublisherStream(requestHandler);
//}

void StreamResponder::endStream(StreamCompletionSignal signal) {
  terminatePublisher();
  StreamStateMachineBase::endStream(signal);
}

void StreamResponder::handleCancel() {
  publisherComplete();
  closeStream(StreamCompletionSignal::CANCEL);
}

void StreamResponder::handleRequestN(uint32_t n) {
  processRequestN(n);
}
}
