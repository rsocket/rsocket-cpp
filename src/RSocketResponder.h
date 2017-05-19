// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Flowable.h"
#include "yarpl/Single.h"
#include "src/Payload.h"

namespace rsocket {

/**
 * Responder APIs to handle requests on an RSocket connection.
 *
 * This is most commonly used by an RSocketServer, but due to the symmetric
 * nature of RSocket, this can be used on the client as well.
 *
 * For context within the overall RSocket protocol:
 *
 * - Client: The side initiating a connection.
 * - Server: The side accepting connections from clients.
 * - Connection: The instance of a transport session between client and server.
 * - Requester: The side sending a request.
 *       A connection has at most 2 Requesters. One in each direction.
 * - Responder: The side receiving a request.
 *       A connection has at most 2 Responders. One in each direction.
 *
 * See https://github.com/rsocket/rsocket/blob/master/Protocol.md#terminology
 * for more information on how this fits into the RSocket protocol terminology.
 */
class RSocketResponder {
 public:
  virtual ~RSocketResponder() = default;

  /**
   * Called when a new `requestResponse` occurs from an RSocketRequester.
   *
   * Return a Single with the response.
   *
   * @param request
   * @param streamId
   * @return
   */
  virtual yarpl::Reference<yarpl::single::Single<rsocket::Payload>>
  handleRequestResponse(rsocket::Payload request, rsocket::StreamId streamId);

  /**
   * Called when a new `requestStream` occurs from an RSocketRequester.
   *
   * Return a Flowable with the response stream.
   *
   * @param request
   * @param streamId
   * @return
   */
  virtual yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestStream(rsocket::Payload request, rsocket::StreamId streamId);

  /**
     * Called when a new `requestChannel` occurs from an RSocketRequester.
     *
     * Return a Flowable with the response stream.
     *
     * @param request
     * @param streamId
     * @return
     */
  virtual yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestChannel(
      rsocket::Payload request,
      yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
          requestStream,
      rsocket::StreamId streamId);

  /**
   * Called when a new `fireAndForget` occurs from an RSocketRequester.
   *
   * No response.
   *
   * @param request
   * @param streamId
   * @return
   */
  virtual void handleFireAndForget(
      rsocket::Payload request,
      rsocket::StreamId streamId);

  /**
   * Called when a new `metadataPush` occurs from an RSocketRequester.
   *
   * No response.
   *
   * @param metadata
   */
  virtual void handleMetadataPush(std::unique_ptr<folly::IOBuf> metadata);

  /// internal method for handling channel requests, not intended to be used
  /// by the application code
  virtual yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
  handleRequestChannelCore(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
      response) noexcept;

  /// internal method for handling stream requests, not intended to be used
  /// by the application code
  virtual void handleRequestStreamCore(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
      response) noexcept;

  /// internal method for handling request-response requests, not intended to be used
  /// by the application code
  virtual void handleRequestResponseCore(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
      response) noexcept;
};
}
