// Copyright 2004-present Facebook. All Rights Reserved.

#include "TextRequestHandler.h"
#include <string>
#include "yarpl/Flowable.h"

using namespace rsocket;
using namespace yarpl::flowable;

/// Handles a new inbound Stream requested by the other end.
yarpl::Reference<Flowable<rsocket::Payload>>
TextRequestResponder::handleRequestStream(Payload request, StreamId) {
  LOG(INFO) << "TextRequestResponder.handleRequestStream " << request;

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
