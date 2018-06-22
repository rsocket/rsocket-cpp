// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "TextRequestHandler.h"
#include <sstream>
#include <string>
#include "yarpl/Flowable.h"

using namespace rsocket;
using namespace yarpl::flowable;

/// Handles a new inbound Stream requested by the other end.
std::shared_ptr<Flowable<rsocket::Payload>>
TextRequestResponder::handleRequestStream(Payload request, StreamId) {
  LOG(INFO) << "TextRequestResponder.handleRequestStream " << request;

  // string from payload data
  auto requestString = request.moveDataToString();

  return Flowable<>::range(1, 100)->map(
      [name = std::move(requestString)](int64_t v) {
        std::stringstream ss;
        ss << "Hello " << name << " " << v << "!";
        std::string s = ss.str();
        return Payload(s, "metadata");
      });
}
