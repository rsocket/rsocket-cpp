#pragma once

#include <folly/io/async/EventBase.h>

#include "rsocket/framing/FrameProcessor.h"

namespace rsocket {

// This class is a wrapper around FrameProcessor which ensures all methods of
// FrameProcessor get executed in a particular EventBase.
//
// This is currently used in the server where the resumed Transport of the
// client is on a different EventBase compared to the EventBase on which the
// original RSocketStateMachine was constructed for the client.  Here the
// transport uses this class to schedule events of the RSocketStateMachine
// (FrameProcessor) in the original EventBase.
class ScheduledFrameProcessor : public FrameProcessor {
 public:
  ScheduledFrameProcessor(
      std::shared_ptr<FrameProcessor> fp,
      folly::EventBase* evb)
      : frameProcessor_(std::move(fp)), evb_(evb) {}

  ~ScheduledFrameProcessor();

  void processFrame(std::unique_ptr<folly::IOBuf> ioBuf) override;
  void onTerminal(folly::exception_wrapper ex) override;

 private:
  std::shared_ptr<FrameProcessor> frameProcessor_;
  folly::EventBase* evb_;
};

} // rsocket
