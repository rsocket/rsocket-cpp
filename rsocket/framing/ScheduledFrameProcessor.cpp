#include "rsocket/framing/ScheduledFrameProcessor.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>

namespace rsocket {

ScheduledFrameProcessor::~ScheduledFrameProcessor() {
  // ScheduledFrameProcessor could get destroyed before the scheduled events of
  // the class get executed.  So preserve a copy of inner frameProcessor_ in
  // the event queue.  Since the events in the queue are executed in order we
  // are guaranteed to have the frameProcessor_ during all the other events
  // which were added before the destruction of this class.
  evb_->runInEventBaseThread([frameProcessor = frameProcessor_](){});
}

void ScheduledFrameProcessor::processFrame(
    std::unique_ptr<folly::IOBuf> ioBuf) {
  auto frameProcessorPtr = frameProcessor_.get();
  evb_->runInEventBaseThread(
      [ frameProcessorPtr, ioBuf = std::move(ioBuf) ]() mutable {
        frameProcessorPtr->processFrame(std::move(ioBuf));
      });
}

void ScheduledFrameProcessor::onTerminal(folly::exception_wrapper ex) {
  auto frameProcessorPtr = frameProcessor_.get();
  evb_->runInEventBaseThread(
      [ ex = std::move(ex), frameProcessorPtr ]() mutable {
        frameProcessorPtr->onTerminal(std::move(ex));
      });
}
}
