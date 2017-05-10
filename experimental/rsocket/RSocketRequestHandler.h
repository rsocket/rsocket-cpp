// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>

#include "yarpl/Flowable.h"
#include "yarpl/Flowables.h"

#include "src/Payload.h"
#include "src/StreamState.h"

namespace rsocket {

/**
 * RequestHandler APIs to handle requests on an RSocket connection.
 *
 * This is most commonly used by an RSocketServer, but due to the symmetric
 * nature of RSocket, this can be used on the client as well.
 */
class RSocketRequestHandler {
 public:
  /**
   * Called when a new `requestStream` occurs from an RSocketRequester.
   *
   * Return a Flowable with the response stream.
   *
   * @param request
   * @param streamId
   * @return
   */
  virtual yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
  handleRequestStream(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId) {
    return yarpl::flowable::Flowables::error<reactivesocket::Payload>(
        std::make_exception_ptr(
            std::logic_error("handleRequestStream not implemented")));
  }

  /**
     * Called when a new `requestChannel` occurs from an RSocketRequester.
     *
     * Return a Flowable with the response stream.
     *
     * @param request
     * @param streamId
     * @return
     */
  virtual yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
  handleRequestChannel(
      yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
          requestStream,
      reactivesocket::StreamId streamId) {
    return yarpl::flowable::Flowables::error<reactivesocket::Payload>(
        std::make_exception_ptr(
            std::logic_error("handleRequestChannel not implemented")));
  }
};
}
