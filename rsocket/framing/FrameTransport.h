// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <memory>
#include <mutex>

#include <folly/ExceptionWrapper.h>
#include <folly/Optional.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/internal/AllowanceSemaphore.h"
#include "rsocket/internal/Common.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

class FrameProcessor;

class FrameTransport final :
    /// Registered as an input in the DuplexConnection.
    public DuplexConnection::Subscriber,
    /// Receives signals about connection writability.
    public yarpl::flowable::Subscription {
 public:
  explicit FrameTransport(std::unique_ptr<DuplexConnection> connection);
  ~FrameTransport();

  void setFrameProcessor(std::shared_ptr<FrameProcessor>);

  /// Writes the frame directly to output. If the connection was closed it will
  /// drop the frame.
  void outputFrameOrDrop(std::unique_ptr<folly::IOBuf>);

  /// Cancel the input, complete the output, and close the underlying
  /// connection.
  void close();

  /// Cancel the input, error the output, and close the underlying connection.
  /// This must be closed with a non-empty exception_wrapper.
  void closeWithError(folly::exception_wrapper);

  bool isClosed() const {
    return !connection_;
  }

 private:
  // TODO(t15924567): Recursive locks are evil! This should instead use a
  // synchronization abstraction which preserves FIFO ordering. However, this is
  // incrementally better than the race conditions which existed here before.
  //
  // Further reading:
  // https://groups.google.com/forum/?hl=en#!topic/comp.programming.threads/tcrTKnfP8HI%5B1-25%5D
  using Mutex = std::recursive_mutex;
  using Lock = std::lock_guard<Mutex>;

  void connect();

  // Subscriber.

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>) override;
  void onNext(std::unique_ptr<folly::IOBuf>) override;
  void onComplete() override;
  void onError(std::exception_ptr) override;

  // Subscription.

  void request(int64_t) override;
  void cancel() override;

  /// Drain all pending reads and any pending terminal signal into the
  /// FrameProcessor.
  ///
  /// TODO: This always sends the payloads first and then follows with the
  /// terminal signal, regardless if terminal signal was sent before the
  /// payloads.  Not clear if that is desirable.
  void drainReads(const Lock&);

  /// Terminates the FrameProcessor.  Will queue up the exception if no
  /// processor is set, overwriting any previously queued exception.
  void terminateProcessor(folly::exception_wrapper);

  void closeImpl(folly::exception_wrapper);

  mutable Mutex mutex_;

  std::shared_ptr<FrameProcessor> frameProcessor_;

  std::shared_ptr<DuplexConnection> connection_;

  yarpl::Reference<DuplexConnection::Subscriber> connectionOutput_;
  yarpl::Reference<yarpl::flowable::Subscription> connectionInputSub_;

  std::deque<std::unique_ptr<folly::IOBuf>> pendingReads_;
  folly::Optional<folly::exception_wrapper> pendingTerminal_;
};
}
