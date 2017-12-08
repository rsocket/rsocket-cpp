#pragma once

#include <folly/io/async/EventBase.h>

#include "rsocket/framing/FrameTransportImpl.h"
#include "rsocket/framing/ScheduledFrameProcessor.h"

namespace rsocket {

// This class is a wrapper around FrameTransport which ensures all methods of
// FrameTransport get executed in a particular EventBase.
//
// This is currently used in the server where the resumed Transport of the
// client is on a different EventBase compared to the EventBase on which the
// original RSocketStateMachine was constructed for the client.  Here the
// RSocketStateMachine uses this class to schedule events of the Transport in
// the new EventBase.
class ScheduledFrameTransport : public FrameTransport,
                                public yarpl::enable_get_ref {
 public:
  ScheduledFrameTransport(
      yarpl::Reference<FrameTransport> frameTransport,
      folly::EventBase* transportEvb,
      folly::EventBase* stateMachineEvb)
      : transportEvb_(transportEvb),
        stateMachineEvb_(stateMachineEvb),
        frameTransport_(std::move(frameTransport)) {}

  ~ScheduledFrameTransport();

  void setFrameProcessor(std::shared_ptr<FrameProcessor> fp) override;
  void outputFrameOrDrop(std::unique_ptr<folly::IOBuf> ioBuf) override;
  void close() override;

 private:
  DuplexConnection* getConnection() override {
    DLOG(FATAL)
        << "ScheduledFrameTransport doesn't support getConnection method, "
            "because it can create safe usage issues when EventBase of the "
            "transport and the RSocketClient is not the same.";
    return nullptr;
  }

 private:
  folly::EventBase* transportEvb_;
  folly::EventBase* stateMachineEvb_;
  yarpl::Reference<FrameTransport> frameTransport_;
};
}
