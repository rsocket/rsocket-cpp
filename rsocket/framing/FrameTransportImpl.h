// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "rsocket/DuplexConnection.h"
#include "rsocket/internal/Common.h"
#include "yarpl/flowable/Subscription.h"

#include "rsocket/framing/FrameTransport.h"

namespace rsocket {

class FrameProcessor;

class FrameTransportImpl final
    : public FrameTransport,
      /// Registered as an input in the DuplexConnection.
      public DuplexConnection::Subscriber,
      /// Receives signals about connection writability.
      public yarpl::flowable::Subscription {
 public:
  explicit FrameTransportImpl(std::unique_ptr<DuplexConnection> connection);
  ~FrameTransportImpl();

  void setFrameProcessor(std::shared_ptr<FrameProcessor>) override;

  /// Writes the frame directly to output. If the connection was closed it will
  /// drop the frame.
  void outputFrameOrDrop(std::unique_ptr<folly::IOBuf>) override;

  /// Cancel the input, complete the output, and close the underlying
  /// connection.
  void close() override;

  /// Cancel the input, error the output, and close the underlying connection.
  /// This must be closed with a non-empty exception_wrapper.
  void closeWithError(folly::exception_wrapper) override;

  bool isClosed() const {
    return !connection_;
  }

 private:
  void connect();

  // Subscriber.

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>) override;
  void onNext(std::unique_ptr<folly::IOBuf>) override;
  void onComplete() override;
  void onError(folly::exception_wrapper) override;

  // Subscription.

  void request(int64_t) override;
  void cancel() override;

  /// Terminates the FrameProcessor.  Will queue up the exception if no
  /// processor is set, overwriting any previously queued exception.
  void terminateProcessor(folly::exception_wrapper);

  void closeImpl(folly::exception_wrapper);

  std::shared_ptr<FrameProcessor> frameProcessor_;
  std::shared_ptr<DuplexConnection> connection_;

  yarpl::Reference<DuplexConnection::Subscriber> connectionOutput_;
  yarpl::Reference<yarpl::flowable::Subscription> connectionInputSub_;
};
}
