// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Conv.h>

#include "rsocket/ColdResumeHandler.h"

using namespace yarpl;
using namespace yarpl::flowable;

namespace {

class CancelingSubscriber : public Subscriber<rsocket::Payload> {
  void onSubscribe(Reference<Subscription> subscription) override {
    Subscriber<rsocket::Payload>::onSubscribe(subscription);
    Subscriber<rsocket::Payload>::subscription()->cancel();
  }

  void onNext(rsocket::Payload) override {}
};
}

namespace rsocket {

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
