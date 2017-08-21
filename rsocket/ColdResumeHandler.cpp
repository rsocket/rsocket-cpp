// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/ColdResumeHandler.h"

#include "yarpl/flowable/CancelingSubscriber.h"

#include <folly/Conv.h>

using namespace yarpl;
using namespace yarpl::flowable;

namespace rsocket {

std::string ColdResumeHandler::generateStreamToken(
    const Payload&,
    StreamId streamId,
    StreamType) {
  return folly::to<std::string>(streamId);
}

Reference<Flowable<Payload>> ColdResumeHandler::handleResponderResumeStream(
    std::string /* streamToken */,
    size_t /* publisherAllowance */) {
  return Flowables::error<Payload>(
      std::logic_error("ResumeHandler method not implemented"));
}

Reference<Subscriber<Payload>> ColdResumeHandler::handleRequesterResumeStream(
    std::string /* streamToken */,
    size_t /* consumerAllowance */) {
  return yarpl::make_ref<CancelingSubscriber<Payload>>();
}
}
