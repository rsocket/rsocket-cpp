// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/framing/FrameTransport.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

#include "src/DuplexConnection.h"
#include "src/framing/FrameProcessor.h"

namespace rsocket {

using namespace yarpl::flowable;

FrameTransport::FrameTransport(std::unique_ptr<DuplexConnection> connection)
    : connection_(std::move(connection)) {
  CHECK(connection_);
}

FrameTransport::~FrameTransport() {
  VLOG(6) << "~FrameTransport";
}

void FrameTransport::connect() {
  Lock lock(mutex_);

  DCHECK(connection_);

  if (connectionOutput_) {
    // Already connected.
    return;
  }

  connectionOutput_ = connection_->getOutput();
  connectionOutput_->onSubscribe(yarpl::get_ref(this));

  // The onSubscribe call on the previous line may have called the terminating
  // signal which would call disconnect/close.
  if (connection_) {
    // This may call ::onSubscribe in-line, which calls ::request on the
    // provided subscription, which might deliver frames in-line.  It can also
    // call onComplete which will call disconnect/close and reset the
    // connection_ while still inside of the connection_::setInput method.  We
    // will create a hard reference for that case and keep the object alive
    // until setInput method returns
    auto connectionCopy = connection_;
    connectionCopy->setInput(yarpl::get_ref(this));
  }
}

void FrameTransport::setFrameProcessor(
    std::shared_ptr<FrameProcessor> frameProcessor) {
  Lock lock(mutex_);

  frameProcessor_ = std::move(frameProcessor);
  if (frameProcessor_) {
    CHECK(!isClosed());
    connect();
  }

  drainWrites(lock);
  drainReads(lock);
}

void FrameTransport::close() {
  closeImpl(folly::exception_wrapper());
}

void FrameTransport::closeWithError(folly::exception_wrapper ew) {
  if (!ew) {
    VLOG(1) << "FrameTransport::closeWithError() called with empty exception";
    ew = std::runtime_error("Undefined error");
  }
  closeImpl(std::move(ew));
}

void FrameTransport::closeImpl(folly::exception_wrapper ew) {
  Lock lock(mutex_);

  // Make sure we never try to call back into the processor.
  frameProcessor_ = nullptr;

  if (!connection_) {
    return;
  }

  auto oldConnection = std::move(connection_);

  // Send terminal signals to the DuplexConnection's input and output before
  // tearing it down.  We must do this per DuplexConnection specification (see
  // interface definition).
  if (auto subscriber = std::move(connectionOutput_)) {
    if (ew) {
      subscriber->onError(ew.to_exception_ptr());
    } else {
      subscriber->onComplete();
    }
  }
  if (auto subscription = std::move(connectionInputSub_)) {
    subscription->cancel();
  }
}

void FrameTransport::onSubscribe(yarpl::Reference<Subscription> subscription) {
  Lock lock(mutex_);

  if (!connection_) {
    return;
  }

  CHECK(!connectionInputSub_);
  CHECK(frameProcessor_);
  connectionInputSub_ = std::move(subscription);
  connectionInputSub_->request(kMaxRequestN);
}

void FrameTransport::onNext(std::unique_ptr<folly::IOBuf> frame) {
  Lock lock(mutex_);

  if (connection_ && frameProcessor_) {
    frameProcessor_->processFrame(std::move(frame));
  } else {
    pendingReads_.emplace_back(std::move(frame));
  }
}

void FrameTransport::terminateProcessor(folly::exception_wrapper ex) {
  // This method can be executed multiple times while terminating.

  std::shared_ptr<FrameProcessor> frameProcessor;
  {
    Lock lock(mutex_);
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

void FrameTransport::onComplete() {
  VLOG(6) << "onComplete";
  terminateProcessor(folly::exception_wrapper());
}

void FrameTransport::onError(std::exception_ptr eptr) {
  VLOG(6) << "onError" << folly::exceptionStr(eptr);

  try {
    std::rethrow_exception(eptr);
  } catch (const std::exception& exn) {
    folly::exception_wrapper ew{std::move(eptr), exn};
    terminateProcessor(std::move(ew));
  }
}

void FrameTransport::request(int64_t n) {
  Lock lock(mutex_);

  if (!connection_) {
    // request(n) can be delivered during disconnecting.  We don't care for it
    // anymore.
    return;
  }

  if (writeAllowance_.release(n) > 0) {
    // There are no pending wfrites or we already have this method on the
    // stack.
    return;
  }

  drainWrites(lock);
}

void FrameTransport::cancel() {
  VLOG(6) << "cancel";
  terminateProcessor(folly::exception_wrapper());
}

void FrameTransport::outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf> frame) {
  Lock lock(mutex_);

  // We allow sending frames even without a frame processor so it's possible to
  // send terminal frames without expecting anything in return.
  if (connection_) {
    drainWrites(lock);
    if (pendingWrites_.empty() && writeAllowance_.tryAcquire()) {
      connectionOutput_->onNext(std::move(frame));
      return;
    }
  }

  // We either have no allowance to perform the operation, or the queue has not
  // been drained (e.g. we're looping in ::request), or we are disconnected.
  pendingWrites_.emplace_back(std::move(frame));
}

void FrameTransport::drainReads(const FrameTransport::Lock&) {
  if (!frameProcessor_) {
    return;
  }

  while (!pendingReads_.empty()) {
    auto frame = std::move(pendingReads_.front());
    pendingReads_.pop_front();
    frameProcessor_->processFrame(std::move(frame));
  }

  if (pendingTerminal_) {
    terminateProcessor(std::move(*pendingTerminal_));
    pendingTerminal_ = folly::none;
  }
}

void FrameTransport::drainWrites(const FrameTransport::Lock&) {
  if (!connection_) {
    return;
  }

  // Drain the queue or the allowance.
  while (!pendingWrites_.empty() && writeAllowance_.tryAcquire()) {
    auto frame = std::move(pendingWrites_.front());
    pendingWrites_.pop_front();
    connectionOutput_->onNext(std::move(frame));
  }
}
}
