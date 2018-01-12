// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>

#include "yarpl/Flowable.h"
#include "yarpl/Single.h"

#include "rsocket/Payload.h"
#include "rsocket/statemachine/RSocketStateMachine.h"

#include "rs/publisher.h"

namespace rsocket {

namespace detail {

template <typename T, typename ShkSubscriber>
struct RsRequestStreamSubscriber : yarpl::flowable::BaseSubscriber<T> {
  RsRequestStreamSubscriber(ShkSubscriber&& sub, folly::EventBase* evb)
      : shkSubscriber_(std::move(sub)), evb_(evb) {}

  void onSubscribeImpl() override final {
    evb_->runInEventBaseThread([=] {
      VLOG(0) << "got onSubscribeImpl()";
      wasSubscribed_ = true;
      auto tmp = reqBacklog_;
      reqBacklog_ = 0;

      if (canceled_) {
        return;
      } else {
        VLOG(0) << "calling deferred request(" << tmp << ")";
        this->request(tmp);
      }
    });
  }

  void onNextImpl(T t) override final {
    VLOG(0) << "got OnNext";
    shkSubscriber_.OnNext(std::move(t));
  }
  void onCompleteImpl() override final {
    VLOG(0) << "got OnComplete";
    shkSubscriber_.OnComplete();
  }
  void onErrorImpl(folly::exception_wrapper e) override final {
    VLOG(0) << "got OnError";
    shkSubscriber_.OnError(std::exception_ptr(e.to_exception_ptr()));
  }

  void requestFromRs(int64_t req) {
    VLOG(0) << "requestFromRs(" << req << ")";
    evb_->runInEventBaseThread([=] {
      if (!wasSubscribed_) {
        reqBacklog_ += req;
      } else if (!canceled_) {
        VLOG(0) << "calling immediate request(" << req << ")";
        this->request(req);
      }
    });
  }

  void cancelFromRs() {
    evb_->runInEventBaseThread([=] {
      VLOG(0) << "calling cancel()";
      canceled_ = true;
    });
  }

 private:
  ShkSubscriber shkSubscriber_;
  bool wasSubscribed_{false};
  folly::EventBase* evb_;
  int64_t reqBacklog_{0};
  bool canceled_{false};
};

} // namespace detail

/**
 * Request APIs to submit requests on an RSocket connection.
 *
 * This is most commonly used by an RSocketClient, but due to the symmetric
 * nature of RSocket, this can be used from server->client as well.
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
class RSocketRequester {
 public:
  RSocketRequester(
      std::shared_ptr<rsocket::RSocketStateMachine> srs,
      folly::EventBase& eventBase);

  virtual ~RSocketRequester(); // implementing for logging right now

  RSocketRequester(const RSocketRequester&) = delete;
  RSocketRequester(RSocketRequester&&) = delete;

  RSocketRequester& operator=(const RSocketRequester&) = delete;
  RSocketRequester& operator=(RSocketRequester&&) = delete;

  /**
   * Send a single request and get a response stream.
   *
   * Interaction model details can be found at
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#request-stream
   */
  virtual std::shared_ptr<yarpl::flowable::Flowable<rsocket::Payload>>
  requestStream(rsocket::Payload request);

  template <typename PayloadGen>
  auto rsRequestStream(PayloadGen&& payloadGen) {
    return shk::MakePublisher([payloadGen = std::move(payloadGen),
                               srs = stateMachine_,
                               eb = eventBase_](auto&& rsSubscriber) {
      auto yarplSubscriber = yarpl::make_ref<detail::RsRequestStreamSubscriber<
          rsocket::Payload,
          std::decay_t<decltype(rsSubscriber)>>>(
          std::forward<decltype(rsSubscriber)>(rsSubscriber), eb);

      auto payload = payloadGen();
      eb->runInEventBaseThread([srs = std::move(srs),
                                yarplSubscriber,
                                payload = std::move(payload)]() mutable {
        VLOG(0) << "Calling createStreamRequester()";
        srs->streamsFactory().createStreamRequester(
            std::move(payload), yarplSubscriber);
      });

      return shk::MakeSubscription(
          // request(n)
          [yarplSubscriber](shk::ElementCount n) {
            yarplSubscriber->requestFromRs(n.Get());
          },
          // cancel
          [yarplSubscriber] { yarplSubscriber->cancelFromRs(); });
    });
  }

  /**
   * Start a channel (streams in both directions).
   *
   * Interaction model details can be found at
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#request-channel
   */
  virtual std::shared_ptr<yarpl::flowable::Flowable<rsocket::Payload>>
  requestChannel(
      std::shared_ptr<yarpl::flowable::Flowable<rsocket::Payload>> requests);

  /**
   * Send a single request and get a single response.
   *
   * Interaction model details can be found at
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#stream-sequences-request-response
   */
  virtual std::shared_ptr<yarpl::single::Single<rsocket::Payload>>
  requestResponse(rsocket::Payload request);

  /**
   * Send a single Payload with no response.
   *
   * The returned Single<void> invokes onSuccess or onError
   * based on client-side success or failure. Once the payload is
   * sent to the network it is "forgotten" and the Single<void> will
   * be finished with no further response indicating success
   * or failure on the server.
   *
   * Interaction model details can be found at
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#request-fire-n-forget
   */
  virtual std::shared_ptr<yarpl::single::Single<void>> fireAndForget(
      rsocket::Payload request);

  /**
   * Send metadata without response.
   */
  virtual void metadataPush(std::unique_ptr<folly::IOBuf> metadata);

  /**
   * To be used only temporarily to check the transport's status.
   */
  virtual DuplexConnection* getConnection();

  virtual void closeSocket();

 protected:
  std::shared_ptr<rsocket::RSocketStateMachine> stateMachine_;
  folly::EventBase* eventBase_;
};
}
