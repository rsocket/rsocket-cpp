// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <memory>
#include "src/AllowanceSemaphore.h"
#include "src/Common.h"
#include "src/FrameProcessor.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/SmartPointers.h"

namespace reactivesocket {

class DuplexConnection;

class FrameTransport :
    /// Registered as an input in the DuplexConnection.
    public Subscriber<std::unique_ptr<folly::IOBuf>>,
    /// Receives signals about connection writability.
    public Subscription,
    public std::enable_shared_from_this<FrameTransport> {
 public:
  static std::shared_ptr<FrameTransport> fromDuplexConnection(
      std::unique_ptr<DuplexConnection> connection);
  ~FrameTransport();

  void setFrameProcessor(std::shared_ptr<FrameProcessor>);

  /// Enqueues provided frame to be written to the underlying connection.
  /// Enqueuing a terminal frame does not end the stream.
  ///
  /// This signal corresponds to Subscriber::onNext.
  void outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf> frame);
  void close(folly::exception_wrapper ex);

  bool isClosed() const {
    return !connection_;
  }

  bool outputQueueEmpty() const {
    return pendingWrites_.empty();
  }

 private:
  void connect(std::unique_ptr<DuplexConnection> connection);

  void onSubscribe(std::shared_ptr<Subscription>) override;
  void onNext(std::unique_ptr<folly::IOBuf>) override;
  void onComplete() override;
  void onError(folly::exception_wrapper) override;

  void request(size_t) override;
  void cancel() override;

  void drainOutputFramesQueue();

  void terminateFrameProcessor(
      folly::exception_wrapper,
      StreamCompletionSignal);

  std::shared_ptr<FrameProcessor> frameProcessor_;

  AllowanceSemaphore writeAllowance_;
  std::shared_ptr<DuplexConnection> connection_;

  reactivestreams::SubscriberPtr<
      reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      connectionOutput_;
  reactivestreams::SubscriptionPtr<Subscription> connectionInputSub_;

  std::deque<std::unique_ptr<folly::IOBuf>> pendingWrites_;
};
} // reactivesocekt