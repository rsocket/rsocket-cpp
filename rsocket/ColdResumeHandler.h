// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Flowable.h"

#include "rsocket/Payload.h"
#include "rsocket/internal/Common.h"

namespace rsocket {

// This class has to be implemented by the client application for cold
// resumption.  The default implementation will error/close the streams.
class ColdResumeHandler {
 public:
  // Generate an application-aware streamToken for the given stream parameters.
  virtual std::string generateStreamToken(const Payload&, StreamId, StreamType);

  // This method will be called for each REQUEST_STREAM for which the
  // application acted as a responder.  The default action would be to return a
  // Flowable which errors out immediately.
  // The second parameter is the allowance which the application received
  // before cold-start and hasn't been fulfilled yet.
  virtual yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
  handleResponderResumeStream(
      std::string streamToken,
      uint32_t publisherAllowance);

  // This method will be called for each REQUEST_STREAM for which the
  // application acted as a requester.  The default action would be to return a
  // Subscriber which cancels the stream immediately after getting subscribed.
  // The second parameter is the allowance which the application requested
  // before cold-start and hasn't been fulfilled yet.
  virtual yarpl::Reference<yarpl::flowable::Subscriber<rsocket::Payload>>
  handleRequesterResumeStream(
      std::string streamToken,
      uint32_t consumerAllowance);
};
}
