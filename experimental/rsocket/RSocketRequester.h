// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>
#include "src/ReactiveStreamsCompat.h"
#include "src/StandardReactiveSocket.h"

using namespace ::reactivesocket;
using namespace folly;

namespace rsocket {
class RSocketRequester {
 public:
  static std::shared_ptr<RSocketRequester> create(
      std::unique_ptr<StandardReactiveSocket> srs,
      EventBase& executor);

  ~RSocketRequester();
  RSocketRequester(const RSocketRequester&) = delete; // copy
  RSocketRequester(RSocketRequester&&) = delete; // move
  RSocketRequester& operator=(const RSocketRequester&) = delete; // copy
  RSocketRequester& operator=(RSocketRequester&&) = delete; // move

  // TODO why is everything in here a shared_ptr and not just unique_ptr?

  std::shared_ptr<Subscriber<Payload>> requestChannel(
      std::shared_ptr<Subscriber<Payload>> responseSink);

  void requestStream(
      Payload payload,
      std::shared_ptr<Subscriber<Payload>> responseSink);

  void requestResponse(
      Payload payload,
      std::shared_ptr<Subscriber<Payload>> responseSink);

  void requestFireAndForget(Payload request);

  void metadataPush(std::unique_ptr<folly::IOBuf> metadata);

 private:
  RSocketRequester(
      std::unique_ptr<StandardReactiveSocket> srs,
      EventBase& eventBase);
  std::shared_ptr<StandardReactiveSocket> standardReactiveSocket_;
  EventBase& eventBase_;
};
}