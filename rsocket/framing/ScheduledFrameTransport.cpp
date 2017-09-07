#include "rsocket/framing/ScheduledFrameTransport.h"

#include <folly/io/IOBuf.h>

namespace rsocket {

ScheduledFrameTransport::~ScheduledFrameTransport() {
  // ScheduledFrameTransport could get destroyed before the scheduled events of
  // the class get executed.  So preserve a copy of inner frameProcessor_ in
  // the event queue.  Since the events in the queue are executed in order we
  // are guaranteed to have the frameTransport_ during all the other events
  // which were added before the destruction of this class.
  transportEvb_->runInEventBaseThread([frameTransport = frameTransport_](){});
}

void ScheduledFrameTransport::setFrameProcessor(
    std::shared_ptr<FrameProcessor> fp) {
  transportEvb_->runInEventBaseThread([ this, fp = std::move(fp) ]() mutable {
    auto scheduledFP = std::make_shared<ScheduledFrameProcessor>(
        std::move(fp), stateMachineEvb_);
    frameTransport_->setFrameProcessor(std::move(scheduledFP));
  });
}

void ScheduledFrameTransport::outputFrameOrDrop(
    std::unique_ptr<folly::IOBuf> ioBuf) {
  auto frameTransportPtr = frameTransport_.get();
  transportEvb_->runInEventBaseThread(
      [ frameTransportPtr, ioBuf = std::move(ioBuf) ]() mutable {
        frameTransportPtr->outputFrameOrDrop(std::move(ioBuf));
      });
}

void ScheduledFrameTransport::close() {
  auto frameTransportPtr = frameTransport_.get();
  transportEvb_->runInEventBaseThread(
      [frameTransportPtr]() { frameTransportPtr->close(); });
}

void ScheduledFrameTransport::closeWithError(folly::exception_wrapper ex) {
  auto frameTransportPtr = frameTransport_.get();
  transportEvb_->runInEventBaseThread(
      [ frameTransportPtr, ex = std::move(ex) ]() mutable {
        frameTransportPtr->closeWithError(std::move(ex));
      });
}

} // rsocket
