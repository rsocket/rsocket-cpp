// Copyright 2004-present Facebook. All Rights Reserved.

#include "JsonRequestHandler.h"
#include <string>
#include "yarpl/v/Flowable.h"
#include "yarpl/v/Flowables.h"

using namespace reactivesocket;
using namespace rsocket;
using namespace yarpl;

/// Handles a new inbound Stream requested by the other end.
yarpl::Reference<yarpl::Flowable<reactivesocket::Payload>>
JsonRequestHandler::handleRequestStream(Payload request, StreamId streamId) {
  LOG(INFO) << "JsonRequestHandler.handleRequestStream " << request;

  // string from payload data
  const char* p = reinterpret_cast<const char*>(request.data->data());
  auto requestString = std::string(p, request.data->length());

  return Flowables::range(1, 100)->map([name = std::move(requestString)](
      int64_t v) {
    std::stringstream ss;
    ss << "Hello (should be JSON) " << name << " " << v << "!";
    std::string s = ss.str();
    return Payload(s, "metadata");
  });
}
