#include "rsocket/framing/ScheduledFrameTransport.h"

#include <folly/io/IOBuf.h>

namespace rsocket {

ScheduledFrameTransport::~ScheduledFrameTransport() {}

void ScheduledFrameTransport::setFrameProcessor(
    std::shared_ptr<FrameProcessor> fp) {
  transportEvb_->runInEventBaseThread(
      [ this, self = this->ref_from_this(this), fp = std::move(fp) ]() mutable {
        auto scheduledFP = std::make_shared<ScheduledFrameProcessor>(
            std::move(fp), stateMachineEvb_);
        frameTransport_->setFrameProcessor(std::move(scheduledFP));
      });
}

void ScheduledFrameTransport::outputFrameOrDrop(
    std::unique_ptr<folly::IOBuf> ioBuf) {
  transportEvb_->runInEventBaseThread(
      [ ft = frameTransport_, ioBuf = std::move(ioBuf) ]() mutable {
        ft->outputFrameOrDrop(std::move(ioBuf));
      });
}

void ScheduledFrameTransport::close() {
  transportEvb_->runInEventBaseThread([ft = frameTransport_]() {
    ft->close();
  });
}

} // rsocket
