// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/RSocketRequester.h"

#include <folly/ExceptionWrapper.h>

#include "src/internal/ScheduledSingleObserver.h"
#include "src/internal/ScheduledSubscriber.h"
#include "yarpl/Flowable.h"

using namespace folly;
using namespace yarpl;

namespace rsocket {

RSocketRequester::RSocketRequester(
    std::shared_ptr<RSocketStateMachine> srs,
    EventBase& eventBase)
    : stateMachine_(std::move(srs)), eventBase_(eventBase) {}

RSocketRequester::~RSocketRequester() {
  VLOG(1) << "Destroying RSocketRequester";

  if (stateMachine_) {
    eventBase_.add([stateMachine = std::move(stateMachine_)] {
      VLOG(2) << "Releasing RSocketStateMachine on EventBase";
    });
  }
}

void RSocketRequester::closeSocket() {
  eventBase_.add([stateMachine = std::move(stateMachine_)]{
    VLOG(2) << "Closing RSocketStateMachine on EventBase";
    stateMachine->close(
        folly::exception_wrapper(), StreamCompletionSignal::SOCKET_CLOSED);
  });
}

yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
RSocketRequester::requestChannel(
    yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
    requestStream) {
  CHECK(stateMachine_); // verify the socket was not closed

  return yarpl::flowable::Flowables::fromPublisher<Payload>([
      eb = &eventBase_,
      requestStream = std::move(requestStream),
      srs = stateMachine_
  ](yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
  subscriber) mutable {
    eb->runInEventBaseThread([
        requestStream = std::move(requestStream),
        subscriber = std::move(subscriber),
        srs = std::move(srs),
            eb
    ]() mutable {
      auto responseSink = srs->streamsFactory().createChannelRequester(
          yarpl::make_ref<ScheduledSubscriptionSubscriber<Payload>>(
              std::move(subscriber), *eb));
      // responseSink is wrapped with thread scheduling
      // so all emissions happen on the right thread
      requestStream->subscribe(
          yarpl::make_ref<ScheduledSubscriber<Payload>>(std::move(responseSink),
                                                        *eb));
    });
  });
}

yarpl::Reference<yarpl::flowable::Flowable<Payload>>
RSocketRequester::requestStream(Payload request) {
  CHECK(stateMachine_); // verify the socket was not closed

  return yarpl::flowable::Flowables::fromPublisher<Payload>([
      eb = &eventBase_,
      request = std::move(request),
      srs = stateMachine_
  ](yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
  subscriber) mutable {
    eb->runInEventBaseThread([
        request = std::move(request),
        subscriber = std::move(subscriber),
        srs = std::move(srs),
            eb
    ]() mutable {
      srs->streamsFactory().createStreamRequester(
          std::move(request),
          yarpl::make_ref<ScheduledSubscriptionSubscriber<Payload>>(
              std::move(subscriber), *eb));
    });
  });
}

yarpl::Reference<yarpl::single::Single<rsocket::Payload>>
RSocketRequester::requestResponse(Payload request) {
  CHECK(stateMachine_); // verify the socket was not closed

  return yarpl::single::Single<Payload>::create(
  [eb = &eventBase_, request = std::move(request), srs = stateMachine_](
      yarpl::Reference<yarpl::single::SingleObserver<Payload>>
  observer) mutable {
    eb->runInEventBaseThread([
        request = std::move(request),
        observer = std::move(observer),
        eb,
        srs = std::move(srs)
    ]() mutable {
      srs->streamsFactory().createRequestResponseRequester(
          std::move(request),
          yarpl::make_ref<ScheduledSubscriptionSingleObserver<Payload>>(
              std::move(observer), *eb));
    });
  });
}

yarpl::Reference<yarpl::single::Single<void>> RSocketRequester::fireAndForget(
    rsocket::Payload request) {
  CHECK(stateMachine_); // verify the socket was not closed

  return yarpl::single::Single<void>::create([
      eb = &eventBase_,
      request = std::move(request),
      srs = stateMachine_
  ](yarpl::Reference<yarpl::single::SingleObserver<void>>
  subscriber) mutable {
    eb->runInEventBaseThread([
        request = std::move(request),
        subscriber = std::move(subscriber),
        srs = std::move(srs)
    ]() mutable {
      // TODO pass in SingleSubscriber for underlying layers to
      // call onSuccess/onError once put on network
      srs->requestFireAndForget(std::move(request));
      // right now just immediately call onSuccess
      subscriber->onSuccess();
    });
  });
}

void RSocketRequester::metadataPush(std::unique_ptr<folly::IOBuf> metadata) {
  CHECK(stateMachine_); // verify the socket was not closed

  eventBase_.runInEventBaseThread(
      [srs = stateMachine_, metadata = std::move(metadata)]() mutable {
        srs->metadataPush(std::move(metadata));
      });
}
}
