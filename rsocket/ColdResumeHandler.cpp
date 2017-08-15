// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/ColdResumeHandler.h"

#include <folly/Conv.h>

using namespace yarpl;
using namespace yarpl::flowable;

namespace rsocket {

namespace {

class CancelingSubscriber : public Subscriber<Payload> {
  void onSubscribe(Reference<Subscription> subscription) override {
    Subscriber<Payload>::onSubscribe(subscription);
    Subscriber<Payload>::subscription()->cancel();
  }

  void onNext(Payload) override {}
};
}

std::string ColdResumeHandler::generateStreamToken(
    const Payload&,
    StreamId streamId,
    StreamType) {
  return folly::to<std::string>(streamId);
}

Reference<Flowable<Payload>> ColdResumeHandler::handleResponderResumeStream(
    std::string /* streamToken */,
    uint32_t /* publisherAllowance */) {
  return Flowables::error<Payload>(
      std::logic_error("ResumeHandler method not implemented"));
}

Reference<Subscriber<Payload>> ColdResumeHandler::handleRequesterResumeStream(
    std::string /* streamToken */,
    uint32_t /* consumerAllowance */) {
  return yarpl::make_ref<CancelingSubscriber>();
}
}
