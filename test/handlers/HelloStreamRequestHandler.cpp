// Copyright 2004-present Facebook. All Rights Reserved.

#include "HelloStreamRequestHandler.h"
#include <sstream>
#include "yarpl/Flowable.h"

using namespace ::rsocket;
using namespace yarpl;
using namespace yarpl::flowable;

namespace rsocket {
namespace tests {
/// Handles a new inbound Stream requested by the other end.
Reference<Flowable<rsocket::Payload>>
HelloStreamRequestHandler::handleRequestStream(
    rsocket::Payload request,
    rsocket::StreamId) {
  LOG(INFO) << "HelloStreamRequestHandler.handleRequestStream " << request;

  // string from payload data
  auto requestString = request.moveDataToString();

  return Flowables::range(1, 10)->map([name = std::move(requestString)](
      int64_t v) {
    std::stringstream ss;
    ss << "Hello " << name << " " << v << "!";
    std::string s = ss.str();
    return Payload(s, "metadata");
  });
}
}
}
