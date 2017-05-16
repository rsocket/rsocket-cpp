// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/transports/tcp/TcpDuplexConnection.h"
#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBufQueue.h>
#include "src/temporary_home/SubscriberBase.h"
#include "src/temporary_home/SubscriptionBase.h"

namespace reactivesocket {
using namespace ::folly;

class TcpReaderWriter : public ::folly::AsyncTransportWrapper::WriteCallback,
                        public ::folly::AsyncTransportWrapper::ReadCallback,
                        public SubscriptionBase,
                        public SubscriberBaseT<std::unique_ptr<folly::IOBuf>> {
 public:
  explicit TcpReaderWriter(
      folly::AsyncSocket::UniquePtr&& socket,
      folly::Executor& executor,
      std::shared_ptr<Stats> stats)
      : ExecutorBase(executor),
        stats_(std::move(stats)),
        socket_(std::move(socket)) {}

  ~TcpReaderWriter() {
    socket_->close();
  }

  void setInput(
      std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
          inputSubscriber) {
    CHECK(!inputSubscriber_);
    inputSubscriber_ = std::move(inputSubscriber);
    inputSubscriber_->onSubscribe(SubscriptionBase::shared_from_this());

    socket_->setReadCB(this);
  }

  const std::shared_ptr<Stats> stats_;

 private:
  void onSubscribeImpl(
      std::shared_ptr<Subscription> subscription) noexcept override {
    // no flow control at tcp level, since we can't know the size of messages
    subscription->request(std::numeric_limits<size_t>::max());
  }

  void onNextImpl(std::unique_ptr<folly::IOBuf> element) noexcept override {
    send(std::move(element));
  }

  void onCompleteImpl() noexcept override {
    closeFromWriter();
  }

  void onErrorImpl(folly::exception_wrapper ex) noexcept override {
    closeFromWriter();
  }

  void requestImpl(size_t n) noexcept override {
    // ignored for now, currently flow control is only at higher layers
  }

  void cancelImpl() noexcept override {
    closeFromReader();
  }

  void send(std::unique_ptr<folly::IOBuf> element) {
    stats_->bytesWritten(element->computeChainDataLength());
    socket_->writeChain(this, std::move(element));
  }

  void closeFromWriter() {
    socket_->close();
  }

  void closeFromReader() {
    socket_->close();
  }

  void writeSuccess() noexcept override {}
  void writeErr(
      size_t bytesWritten,
      const ::folly::AsyncSocketException& ex) noexcept override {
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onError(ex);
    }
  }

  void getReadBuffer(void** bufReturn, size_t* lenReturn) noexcept override {
    std::tie(*bufReturn, *lenReturn) = readBuffer_.preallocate(4096, 4096);
  }

  void readDataAvailable(size_t len) noexcept override {
    readBuffer_.postallocate(len);

    stats_->bytesRead(len);

    if (inputSubscriber_) {
      readBufferAvailable(readBuffer_.split(len));
    }
  }

  void readEOF() noexcept override {
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onComplete();
    }
  }

  void readErr(const folly::AsyncSocketException& ex) noexcept override {
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onError(ex);
    }
  }

  bool isBufferMovable() noexcept override {
    return true;
  }

  void readBufferAvailable(
      std::unique_ptr<folly::IOBuf> readBuf) noexcept override {
    inputSubscriber_->onNext(std::move(readBuf));
  }

  folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
  folly::AsyncSocket::UniquePtr socket_;

  std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      inputSubscriber_;
};

TcpDuplexConnection::TcpDuplexConnection(
    folly::AsyncSocket::UniquePtr&& socket,
    folly::Executor& executor,
    std::shared_ptr<Stats> stats)
    : tcpReaderWriter_(std::make_shared<TcpReaderWriter>(
          std::move(socket),
          executor,
          std::move(stats))) {
  tcpReaderWriter_->stats_->duplexConnectionCreated("tcp", this);
}

TcpDuplexConnection::~TcpDuplexConnection() {
  tcpReaderWriter_->stats_->duplexConnectionClosed("tcp", this);
}

std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
TcpDuplexConnection::getOutput() {
  return tcpReaderWriter_;
}

void TcpDuplexConnection::setInput(
    std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
        inputSubscriber) {
  tcpReaderWriter_->setInput(std::move(inputSubscriber));
}

} // reactivesocket
