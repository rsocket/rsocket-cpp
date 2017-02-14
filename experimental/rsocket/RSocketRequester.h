// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>
#include "src/ReactiveStreamsCompat.h"
#include "src/StandardReactiveSocket.h"

using namespace reactivesocket;

namespace rsocket {

/**
 * Request APIs to submit requests on an RSocket connection.
 *
 * This is most commonly used by an RSocketClient, but due to the symmetric
 * nature of RSocket, this can be used from server->client as well.
 */
class RSocketRequester {
 public:
  static std::shared_ptr<RSocketRequester> create(
      std::unique_ptr<StandardReactiveSocket> srs,
      folly::EventBase& executor);
    // TODO figure out how to use folly::Executor instead of EventBase

  ~RSocketRequester();
  RSocketRequester(const RSocketRequester&) = delete; // copy
  RSocketRequester(RSocketRequester&&) = delete; // move
  RSocketRequester& operator=(const RSocketRequester&) = delete; // copy
  RSocketRequester& operator=(RSocketRequester&&) = delete; // move

  // TODO why is everything in here a shared_ptr and not just unique_ptr?

  /**
   * Send a single request and get a response stream.
   *
   * Interaction model details can be found at
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#request-stream
   *
   * @param payload
   * @param responseSink
   */
  void requestStream(
      Payload payload,
      std::shared_ptr<Subscriber<Payload>> responseSink);

  /**
    * Start a channel (streams in both directions).
    *
    * Interaction model details can be found at
    * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#request-channel
    *
    * @param responseSink
    * @return
    */
  std::shared_ptr<Subscriber<Payload>> requestChannel(
      std::shared_ptr<Subscriber<Payload>> responseSink);

  /**
   * Send a single request and get a single response.
   *
   * Interaction model details can be found at
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#stream-sequences-request-response
   *
   * @param payload
   * @param responseSink
   */
  void requestResponse(
      Payload payload,
      std::shared_ptr<Subscriber<Payload>> responseSink);

  /**
   * Send a single Payload with no response.
   *
   * Interaction model details can be found at
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#request-fire-n-forget
   *
   * @param payload
   */
  void requestFireAndForget(Payload payload);

  /**
   * Send metadata without response.
   *
   * @param metadata
   */
  void metadataPush(std::unique_ptr<folly::IOBuf> metadata);

  // TODO implement
  //  void close();

  // TODO implement versions that return Future/Publisher/Flowable

 private:
  RSocketRequester(
      std::unique_ptr<StandardReactiveSocket> srs,
      folly::EventBase& eventBase);
  std::shared_ptr<StandardReactiveSocket> standardReactiveSocket_;
  folly::EventBase& eventBase_;
};
}