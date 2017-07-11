// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <memory>
#include <mutex>

#include <folly/ExceptionWrapper.h>
#include <folly/Optional.h>

#include "src/Payload.h"
#include "src/framing/FrameProcessor.h"
#include "src/internal/AllowanceSemaphore.h"
#include "src/internal/Common.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

class DuplexConnection;

class FrameTransport :
    /// Registered as an input in the DuplexConnection.
    public yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>,
    /// Receives signals about connection writability.
    public yarpl::flowable::Subscription {
 public:
  explicit FrameTransport(std::unique_ptr<DuplexConnection> connection);
  ~FrameTransport();

  void setFrameProcessor(std::shared_ptr<FrameProcessor>);

  /// Enqueues provided frame to be written to the underlying connection.
  /// Enqueuing a terminal frame does not end the stream.
  ///
  /// This signal corresponds to Subscriber::onNext.
  virtual void outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf> frame);

  /// Cancel the input, complete the output, and close the underlying
  /// connection.
  void close();

  /// Cancel the input, error the output, and close the underlying connection.
  /// This must be closed with a non-empty exception_wrapper.
  void closeWithError(folly::exception_wrapper);

  bool isClosed() const {
    return !connection_;
  }

  bool outputQueueEmpty() const {
    return pendingWrites_.empty();
  }

  DuplexConnection* duplexConnection() const;

 private:
  void connect();

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>) noexcept override;
  void onNext(std::unique_ptr<folly::IOBuf>) noexcept override;
  void onComplete() noexcept override;
  void onError(std::exception_ptr) noexcept override;

  void request(int64_t) noexcept override;
  void cancel() noexcept override;

  void drainOutputFramesQueue();

  void terminateFrameProcessor(folly::exception_wrapper);

  void closeImpl(folly::exception_wrapper);

  // TODO(t15924567): Recursive locks are evil! This should instead use a
  // synchronization abstraction which preserves FIFO ordering. However, this is
  // incrementally better than the race conditions which existed here before.
  //
  // Further reading:
  // https://groups.google.com/forum/?hl=en#!topic/comp.programming.threads/tcrTKnfP8HI%5B1-25%5D
  mutable std::recursive_mutex mutex_;

  std::shared_ptr<FrameProcessor> frameProcessor_;

  AllowanceSemaphore writeAllowance_;
  std::shared_ptr<DuplexConnection> connection_;

  yarpl::Reference<yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>>
      connectionOutput_;
  yarpl::Reference<yarpl::flowable::Subscription> connectionInputSub_;

  std::deque<std::unique_ptr<folly::IOBuf>> pendingWrites_;
  std::deque<std::unique_ptr<folly::IOBuf>> pendingReads_;
  folly::Optional<folly::exception_wrapper> pendingTerminal_;
};
} // reactivesocket
