// Copyright 2004-present Facebook. All Rights Reserved.

#include "HelloStreamRequestHandler.h"
#include <string>
#include "yarpl/Flowable.h"

using namespace ::reactivesocket;
using namespace yarpl;
using namespace yarpl::flowable;

namespace rsocket {
namespace tests {
/// Handles a new inbound Stream requested by the other end.
Reference<Flowable<reactivesocket::Payload>>
HelloStreamRequestHandler::handleRequestStream(
    reactivesocket::Payload request,
    reactivesocket::StreamId streamId) {
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
