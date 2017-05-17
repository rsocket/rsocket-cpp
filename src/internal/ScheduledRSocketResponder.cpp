// Copyright 2004-present Facebook. All Rights Reserved.

#include "ScheduledRSocketResponder.h"
#include <folly/io/async/EventBase.h>
#include "ScheduledSubscriber.h"
#include "ScheduledSingleObserver.h"

namespace rsocket {

ScheduledRSocketResponder::ScheduledRSocketResponder(
      std::shared_ptr<RSocketResponder> inner,
      folly::EventBase& eventBase) : inner_(std::move(inner)), eventBase_(eventBase) {}

  yarpl::Reference<yarpl::single::Single<reactivesocket::Payload>>
  ScheduledRSocketResponder::handleRequestResponse(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId) {
    auto innerFlowable = inner_->handleRequestResponse(std::move(request),
                                                     streamId);
    return yarpl::single::Singles::create<reactivesocket::Payload>(
    [innerFlowable = std::move(innerFlowable), eventBase = &eventBase_](
        yarpl::Reference<yarpl::single::SingleObserver<reactivesocket::Payload>>
    observer) {
      innerFlowable->subscribe(yarpl::make_ref<
          ScheduledSingleObserver<reactivesocket::Payload>>
                                   (std::move(observer), *eventBase));
    });
  }

yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
ScheduledRSocketResponder::handleRequestStream(
    reactivesocket::Payload request,
    reactivesocket::StreamId streamId) {
  auto innerFlowable = inner_->handleRequestStream(std::move(request),
                                                   streamId);
  return yarpl::flowable::Flowables::fromPublisher<reactivesocket::Payload>(
  [innerFlowable = std::move(innerFlowable), eventBase = &eventBase_](
      yarpl::Reference<yarpl::flowable::Subscriber<reactivesocket::Payload>>
  subscriber) {
    innerFlowable->subscribe(yarpl::make_ref<
        ScheduledSubscriber<reactivesocket::Payload>>
    (std::move(subscriber), *eventBase));
  });
}

  yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
  ScheduledRSocketResponder::handleRequestChannel(
      reactivesocket::Payload request,
      yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
      requestStream,
      reactivesocket::StreamId streamId) {
    auto requestStreamFlowable = yarpl::flowable::Flowables::fromPublisher<reactivesocket::Payload>(
    [requestStream = std::move(requestStream), eventBase = &eventBase_](
        yarpl::Reference<yarpl::flowable::Subscriber<reactivesocket::Payload>>
    subscriber) {
      requestStream->subscribe(yarpl::make_ref<
          ScheduledSubscriptionSubscriber<reactivesocket::Payload>>
                                   (std::move(subscriber), *eventBase));
    });
    auto innerFlowable = inner_->handleRequestChannel(std::move(request),
                                                     std::move(requestStreamFlowable),
                                                     streamId);
    return yarpl::flowable::Flowables::fromPublisher<reactivesocket::Payload>(
    [innerFlowable = std::move(innerFlowable), eventBase = &eventBase_](
        yarpl::Reference<yarpl::flowable::Subscriber<reactivesocket::Payload>>
    subscriber) {
      innerFlowable->subscribe(yarpl::make_ref<
          ScheduledSubscriber<reactivesocket::Payload>>
                                   (std::move(subscriber), *eventBase));
    });
  }

void ScheduledRSocketResponder::handleFireAndForget(
    reactivesocket::Payload request,
    reactivesocket::StreamId streamId) {
  inner_->handleFireAndForget(std::move(request), streamId);
}

} // rsocket
