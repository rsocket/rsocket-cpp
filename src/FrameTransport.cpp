// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/FrameTransport.h"
#include <folly/ExceptionWrapper.h>
#include "src/DuplexConnection.h"
#include "src/Frame.h"

namespace reactivesocket {

FrameTransport::FrameTransport(std::unique_ptr<DuplexConnection> connection)
    : connection_(std::move(connection)) {
  CHECK(connection_);
}

FrameTransport::~FrameTransport() {
  VLOG(6) << "~FrameTransport";
}

void FrameTransport::connect() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  DCHECK(connection_);

  if (connectionOutput_) {
    // already connected
    return;
  }

  connectionOutput_ = connection_->getOutput();
  connectionOutput_->onSubscribe(shared_from_this());

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
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  frameProcessor_ = std::move(frameProcessor);
  if (frameProcessor_) {
    CHECK(!isClosed());
    connect();
  }

  drainOutputFramesQueue();
  if (frameProcessor_) {
    while (!pendingReads_.empty()) {
      auto frame = std::move(pendingReads_.front());
      pendingReads_.pop_front();
      frameProcessor_->processFrame(std::move(frame));
    }
    if (pendingTerminal_) {
      terminateFrameProcessor(std::move(*pendingTerminal_));
      pendingTerminal_ = folly::none;
    }
  }
}

void FrameTransport::close(folly::exception_wrapper ex) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  // just making sure we will never try to call back onto the processor
  frameProcessor_ = nullptr;

  if (!connection_) {
    return;
  }

  auto oldConnection = std::move(connection_);

  // Send terminal signals to the DuplexConnection's input and output before
  // tearing it down. We must do this per DuplexConnection specification (see
  // interface definition).
  if (auto subscriber = std::move(connectionOutput_)) {
    if (ex) {
      subscriber->onError(std::move(ex));
    } else {
      subscriber->onComplete();
    }
  }
  if (auto subscription = std::move(connectionInputSub_)) {
    subscription->cancel();
  }
}

void FrameTransport::onSubscribe(
    std::shared_ptr<Subscription> subscription) noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (!connection_) {
    return;
  }

  CHECK(!connectionInputSub_);
  CHECK(frameProcessor_);
  connectionInputSub_ = std::move(subscription);
  connectionInputSub_->request(std::numeric_limits<size_t>::max());
}

void FrameTransport::onNext(std::unique_ptr<folly::IOBuf> frame) noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (connection_ && frameProcessor_) {
    frameProcessor_->processFrame(std::move(frame));
  } else {
    pendingReads_.emplace_back(std::move(frame));
  }
}

void FrameTransport::terminateFrameProcessor(folly::exception_wrapper ex) {
  // this method can be executed multiple times during terminating

  std::shared_ptr<FrameProcessor> frameProcessor;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!frameProcessor_) {
      pendingTerminal_ = std::move(ex);
      return;
    }
    frameProcessor = std::move(frameProcessor_);
  }

  if (frameProcessor) {
    VLOG(3) << this << " terminating frame processor ex=" << ex.what();
    frameProcessor->onTerminal(std::move(ex));
  }
}

void FrameTransport::onComplete() noexcept {
  VLOG(6) << "onComplete";
  terminateFrameProcessor(folly::exception_wrapper());
}

void FrameTransport::onError(folly::exception_wrapper ex) noexcept {
  VLOG(6) << "onError" << ex.what();
  terminateFrameProcessor(std::move(ex));
}

void FrameTransport::request(size_t n) noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

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
  terminateFrameProcessor(folly::exception_wrapper());
}

void FrameTransport::outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf> frame) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  // We allow sending frames even without a frame processor so it's possible
  // to send terminal frames without expecting anything in return
  if (connection_) {
    drainOutputFramesQueue();
    if (pendingWrites_.empty() && writeAllowance_.tryAcquire()) {
      // TODO: temporary disabling VLOG as we don't know the correct
      // frame serializer here. There is refactoring of this class planned
      // which will allow enabling it again.
      // VLOG(3) << this << " writing frame " << FrameHeader::peekType(*frame);

      connectionOutput_->onNext(std::move(frame));
      return;
    }
  }
  // TODO: temporary disabling VLOG as we don't know the correct
  // frame serializer here. There is refactoring of this class planned
  // which will allow enabling it again.
  // VLOG(3) << this << " queuing frame " << FrameHeader::peekType(*frame);

  // We either have no allowance to perform the operation, or the queue has
  // not been drained (e.g. we're looping in ::request).
  // or we are disconnected
  pendingWrites_.emplace_back(std::move(frame));
}

void FrameTransport::drainOutputFramesQueue() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (connection_) {
    // Drain the queue or the allowance.
    while (!pendingWrites_.empty() && writeAllowance_.tryAcquire()) {
      auto frame = std::move(pendingWrites_.front());
      // TODO: temporary disabling VLOG as we don't know the correct
      // frame serializer here. There is refactoring of this class planned
      // which will allow enabling it again.
      // VLOG(3) << this << " flushing frame " << FrameHeader::peekType(*frame);
      pendingWrites_.pop_front();
      connectionOutput_->onNext(std::move(frame));
    }
  }
}

DuplexConnection* FrameTransport::duplexConnection() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return connection_.get();
}

} // reactivesocket
