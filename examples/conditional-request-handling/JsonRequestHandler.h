// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Payload.h"
#include "src/RSocket.h"

class JsonRequestResponder : public rsocket::RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestStream(rsocket::Payload request, rsocket::StreamId streamId)
      override;
};
