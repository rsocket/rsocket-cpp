// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/RSocketResponder.h"

namespace folly {
class EventBase;
}

namespace rsocket {

//
// A decorated RSocketResponder object which schedules the calls from
// application code to RSocket on the provided EventBase
//
class ScheduledRSocketResponder : public RSocketResponder {
 public:
  ScheduledRSocketResponder(
      std::shared_ptr<RSocketResponder> inner,
      folly::EventBase& eventBase);

  yarpl::Reference<yarpl::single::Single<reactivesocket::Payload>>
  handleRequestResponse(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId) override;

  yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
  handleRequestStream(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId) override;

  yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
  handleRequestChannel(
      reactivesocket::Payload request,
      yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
      requestStream,
      reactivesocket::StreamId streamId) override;

  void handleFireAndForget(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId) override;

 private:
  std::shared_ptr<RSocketResponder> inner_;
  folly::EventBase& eventBase_;
};

} // rsocket
