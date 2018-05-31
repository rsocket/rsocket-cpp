// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/ScheduledFrameProcessor.h"

namespace rsocket {

ScheduledFrameProcessor::ScheduledFrameProcessor(
    std::shared_ptr<FrameProcessor> processor,
    folly::EventBase* evb)
    : evb_{evb}, processor_{std::move(processor)} {}

ScheduledFrameProcessor::~ScheduledFrameProcessor() = default;

void ScheduledFrameProcessor::processFrame(
    std::unique_ptr<folly::IOBuf> ioBuf) {
  CHECK(processor_) << "Calling processFrame() after onTerminal()";

  evb_->runInEventBaseThread(
      [processor = processor_, buf = std::move(ioBuf)]() mutable {
        processor->processFrame(std::move(buf));
      });
}

void ScheduledFrameProcessor::onTerminal(folly::exception_wrapper ew) {
  evb_->runInEventBaseThread(
      [e = std::move(ew), processor = std::move(processor_)]() mutable {
        processor->onTerminal(std::move(e));
      });
}

} // namespace rsocket
