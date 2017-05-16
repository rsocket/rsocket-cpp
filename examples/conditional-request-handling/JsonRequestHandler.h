// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/RSocket.h"
#include "src/Payload.h"

class JsonRequestHandler : public rsocket::RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
  handleRequestStream(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId) override;
};
