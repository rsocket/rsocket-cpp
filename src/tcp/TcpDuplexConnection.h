// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <folly/SocketAddress.h>
#include <folly/io/IOBufQueue.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/AsyncTransport.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include <src/Stats.h>
#include "src/DuplexConnection.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/IntrusiveDeleter.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {
class TcpDuplexConnection;

class TcpSubscriptionBase : public virtual ::reactivesocket::IntrusiveDeleter,
                            public ::reactivesocket::Subscription {
 public:
  explicit TcpSubscriptionBase(TcpDuplexConnection& connection)
      : connection_(connection){};

  ~TcpSubscriptionBase() = default;

  // Subscription methods
  void request(size_t n) override;

  void cancel() override;

 private:
  TcpDuplexConnection& connection_;
};

class TcpDuplexConnection;

class TcpOutputSubscriber : public Subscriber<std::unique_ptr<folly::IOBuf>> {
 public:
  explicit TcpOutputSubscriber(TcpDuplexConnection& connection)
      : connection_(connection){};

  void onSubscribe(Subscription& subscription) override;

  void onNext(std::unique_ptr<folly::IOBuf> element) override;

  void onComplete() override;

  void onError(folly::exception_wrapper ex) override;

 private:
  TcpDuplexConnection& connection_;
};

class TcpDuplexConnection
    : public DuplexConnection,
      public ::folly::AsyncTransportWrapper::WriteCallback,
      public ::folly::AsyncTransportWrapper::ReadCallback {
 public:
  explicit TcpDuplexConnection(
      folly::AsyncSocket::UniquePtr&& socket,
      Stats& stats = Stats::noop())
      : socket_(std::move(socket)), stats_(stats) {
    stats_.connectionCreated("tcp", this);
  };

  ~TcpDuplexConnection() {
    socket_->close();
    stats_.connectionClosed("tcp", this);
  };

  Subscriber<std::unique_ptr<folly::IOBuf>>& getOutput() override;

  void setInput(Subscriber<std::unique_ptr<folly::IOBuf>>& framesSink) override;

  void send(std::unique_ptr<folly::IOBuf> element);

  void writeSuccess() noexcept override;

  void writeErr(
      size_t bytesWritten,
      const ::folly::AsyncSocketException& ex) noexcept override;

  void getReadBuffer(void** bufReturn, size_t* lenReturn) noexcept override;

  void readDataAvailable(size_t len) noexcept override;

  void readEOF() noexcept override;

  void readErr(const folly::AsyncSocketException& ex) noexcept override;

  bool isBufferMovable() noexcept override;

  void readBufferAvailable(
      std::unique_ptr<folly::IOBuf> readBuf) noexcept override;

  void closeFromWriter();

  void closeFromReader();

 private:
  folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
  std::unique_ptr<TcpOutputSubscriber> outputSubscriber_;
  SubscriberPtr<Subscriber<std::unique_ptr<folly::IOBuf>>> inputSubscriber_;
  folly::AsyncSocket::UniquePtr socket_;
  Stats& stats_;
};
}
