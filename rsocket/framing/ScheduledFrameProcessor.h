// Copyright 2004-present Facebook. All Rights Reserved.

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
  ScheduledFrameProcessor(std::shared_ptr<FrameProcessor>, folly::EventBase*);
  ~ScheduledFrameProcessor();

  void processFrame(std::unique_ptr<folly::IOBuf>) override;
  void onTerminal(folly::exception_wrapper) override;

 private:
  folly::EventBase* const evb_;
  std::shared_ptr<FrameProcessor> processor_;
};

} // namespace rsocket
