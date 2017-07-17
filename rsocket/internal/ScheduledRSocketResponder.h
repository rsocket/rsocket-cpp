// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/RSocketResponder.h"

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

  yarpl::Reference<yarpl::single::Single<Payload>>
  handleRequestResponse(
      Payload request,
      StreamId streamId) override;

  yarpl::Reference<yarpl::flowable::Flowable<Payload>>
  handleRequestStream(
      Payload request,
      StreamId streamId) override;

  yarpl::Reference<yarpl::flowable::Flowable<Payload>>
  handleRequestChannel(
      Payload request,
      yarpl::Reference<yarpl::flowable::Flowable<Payload>>
      requestStream,
      StreamId streamId) override;

  void handleFireAndForget(
      Payload request,
      StreamId streamId) override;

 private:
  std::shared_ptr<RSocketResponder> inner_;
  folly::EventBase& eventBase_;
};

} // rsocket
