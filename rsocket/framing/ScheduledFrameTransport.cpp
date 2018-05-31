// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/ScheduledFrameTransport.h"

#include "rsocket/framing/ScheduledFrameProcessor.h"

namespace rsocket {

ScheduledFrameTransport::~ScheduledFrameTransport() = default;

void ScheduledFrameTransport::setFrameProcessor(
    std::shared_ptr<FrameProcessor> fp) {
  CHECK(frameTransport_) << "Inner transport already closed";

  transportEvb_->runInEventBaseThread([stateMachineEvb = stateMachineEvb_,
                                       transport = frameTransport_,
                                       fp = std::move(fp)]() mutable {
    auto scheduledFP = std::make_shared<ScheduledFrameProcessor>(
        std::move(fp), stateMachineEvb);
    transport->setFrameProcessor(std::move(scheduledFP));
  });
}

void ScheduledFrameTransport::outputFrameOrDrop(
    std::unique_ptr<folly::IOBuf> ioBuf) {
  CHECK(frameTransport_) << "Inner transport already closed";

  transportEvb_->runInEventBaseThread(
      [transport = frameTransport_, buf = std::move(ioBuf)]() mutable {
        transport->outputFrameOrDrop(std::move(buf));
      });
}

void ScheduledFrameTransport::close() {
  CHECK(frameTransport_) << "Inner transport already closed";

  transportEvb_->runInEventBaseThread(
      [transport = std::move(frameTransport_)]() { transport->close(); });
}

bool ScheduledFrameTransport::isConnectionFramed() const {
  CHECK(frameTransport_) << "Inner transport already closed";
  return frameTransport_->isConnectionFramed();
}

} // namespace rsocket
