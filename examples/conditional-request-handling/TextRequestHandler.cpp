// Copyright 2004-present Facebook. All Rights Reserved.

#include "TextRequestHandler.h"
#include <string>
#include "yarpl/v/Flowable.h"
#include "yarpl/v/Flowables.h"

using namespace reactivesocket;
using namespace rsocket;
using namespace yarpl;

/// Handles a new inbound Stream requested by the other end.
yarpl::Reference<yarpl::Flowable<reactivesocket::Payload>>
TextRequestHandler::handleRequestStream(Payload request, StreamId streamId) {
  LOG(INFO) << "TextRequestHandler.handleRequestStream " << request;

  // string from payload data
  auto requestString = request.moveDataToString();

  return Flowables::range(1, 100)->map([name = std::move(requestString)](
      int64_t v) {
    std::stringstream ss;
    ss << "Hello " << name << " " << v << "!";
    std::string s = ss.str();
    return Payload(s, "metadata");
  });
}
