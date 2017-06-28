// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/statemachine/StreamResponder.h"
#include "yarpl/utils/ExceptionString.h"

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

void StreamResponder::onError(std::exception_ptr ex) noexcept {
  publisherComplete();
  applicationError(yarpl::exceptionStr(ex));
  closeStream(StreamCompletionSignal::ERROR);
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
  closeStream(StreamCompletionSignal::CANCEL);
  publisherComplete();
}

void StreamResponder::handleRequestN(uint32_t n) {
  processRequestN(n);
}
}
