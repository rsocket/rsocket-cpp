// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/FrameTransport.h"

#include <folly/ExceptionWrapper.h>
#include "src/DuplexConnection.h"

namespace reactivesocket {

FrameTransport::FrameTransport(std::unique_ptr<DuplexConnection> connection)
    : connection_(std::move(connection)) {
  CHECK(connection_);
}

FrameTransport::~FrameTransport() {
  VLOG(6) << "~FrameTransport";
}

void FrameTransport::connect() {
  DCHECK(connection_);

  if (connectionOutput_) {
    // already connected
    return;
  }

  connectionOutput_.reset(connection_->getOutput());
  connectionOutput_.onSubscribe(shared_from_this());

  // the onSubscribe call on the previous line may have called the terminating
  // signal which would call disconnect/close
  if (connection_) {
    // This may call ::onSubscribe in-line, which calls ::request on the
    // provided
    // subscription, which might deliver frames in-line.
    // it can also call onComplete which will call disconnect/close and reset
    // the connection_ while still inside of the connection_::setInput method.
    // We will create a hard reference for that case and keep the object alive
    // until setInput method returns
    auto connectionCopy = connection_;
    connectionCopy->setInput(shared_from_this());
  }
}

void FrameTransport::setFrameProcessor(
    std::shared_ptr<FrameProcessor> frameProcessor) {
  frameProcessor_ = std::move(frameProcessor);

  if (frameProcessor_) {
    CHECK(!isClosed());
    connect();
  }

  drainOutputFramesQueue();
}

void FrameTransport::close(folly::exception_wrapper ex) {
  // just making sure we will never try to call back onto the processor
  frameProcessor_ = nullptr;

  if (!connection_) {
    return;
  }

  auto oldConnection = std::move(connection_);

  // Send terminal signals to the DuplexConnection's input and output before
  // tearing it down. We must do this per DuplexConnection specification (see
  // interface definition).
  if (ex) {
    connectionOutput_.onError(std::move(ex));
  } else {
    connectionOutput_.onComplete();
  }
  connectionInputSub_.cancel();
}

void FrameTransport::onSubscribe(
    std::shared_ptr<Subscription> subscription) noexcept {
  CHECK(!connectionInputSub_);
  CHECK(frameProcessor_);
  connectionInputSub_.reset(std::move(subscription));
  connectionInputSub_.request(std::numeric_limits<size_t>::max());
}

void FrameTransport::onNext(std::unique_ptr<folly::IOBuf> frame) noexcept {
  if (connection_) {
    CHECK(frameProcessor_); // if *this is not closed and is pulling frames, it
    // should have frameProcessor
    frameProcessor_->processFrame(std::move(frame));
  }
}

void FrameTransport::terminateFrameProcessor(
    folly::exception_wrapper ex,
    StreamCompletionSignal signal) {
  // this method can be executed multiple times during terminating
  if (frameProcessor_) {
    frameProcessor_->onTerminal(std::move(ex), signal);
    frameProcessor_ = nullptr;
  }
}

void FrameTransport::onComplete() noexcept {
  VLOG(6) << "onComplete";
  terminateFrameProcessor(
      folly::exception_wrapper(), StreamCompletionSignal::CONNECTION_END);
}

void FrameTransport::onError(folly::exception_wrapper ex) noexcept {
  VLOG(6) << "onError" << ex.what();
  terminateFrameProcessor(
      std::move(ex), StreamCompletionSignal::CONNECTION_ERROR);
}

void FrameTransport::request(size_t n) noexcept {
  if (!connection_) {
    // request(n) can be delivered during disconnecting
    // we don't care for it anymore
    return;
  }

  if (writeAllowance_.release(n) > 0) {
    // There are no pending wfrites or we already have this method on the
    // stack.
    return;
  }
  drainOutputFramesQueue();
}

void FrameTransport::cancel() noexcept {
  VLOG(6) << "cancel";
  terminateFrameProcessor(
      folly::exception_wrapper(), StreamCompletionSignal::CONNECTION_END);
}

void FrameTransport::outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf> frame) {
  // we don't want to be sending frames when frameProcessor_ is not set because
  // we wont have a way to process error/terminating signals
  if (connection_ && frameProcessor_) {
    drainOutputFramesQueue();
    if (pendingWrites_.empty() && writeAllowance_.tryAcquire()) {
      connectionOutput_.onNext(std::move(frame));
      return;
    }
  }
  // We either have no allowance to perform the operation, or the queue has
  // not been drained (e.g. we're looping in ::request).
  // or we are disconnected
  pendingWrites_.emplace_back(std::move(frame));
}

void FrameTransport::drainOutputFramesQueue() {
  if (connection_ && frameProcessor_) {
    // Drain the queue or the allowance.
    while (!pendingWrites_.empty() && writeAllowance_.tryAcquire()) {
      auto frame = std::move(pendingWrites_.front());
      pendingWrites_.pop_front();
      connectionOutput_.onNext(std::move(frame));
    }
  }
}

DuplexConnection* FrameTransport::duplexConnection() const {
  return connection_.get();
}

} // reactivesocket
