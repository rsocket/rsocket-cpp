// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/transports/tcp/TcpDuplexConnection.h"
#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBufQueue.h>
#include "src/internal/Common.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

using namespace ::folly;
using namespace yarpl::flowable;

class TcpReaderWriter : public ::folly::AsyncTransportWrapper::WriteCallback,
                        public ::folly::AsyncTransportWrapper::ReadCallback,
                        public std::enable_shared_from_this<TcpReaderWriter> {
 public:
  explicit TcpReaderWriter(
      folly::AsyncSocket::UniquePtr&& socket,
      std::shared_ptr<RSocketStats> stats)
      : socket_(std::move(socket)), stats_(std::move(stats)) {}

  ~TcpReaderWriter() {
    CHECK(isClosed());
    DCHECK(!inputSubscriber_);
  }

  void setInput(
      yarpl::Reference<rsocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
          inputSubscriber) {
    if (inputSubscriber && isClosed()) {
      inputSubscriber->onComplete();
      return;
    }

    if(!inputSubscriber) {
      inputSubscriber_ = nullptr;
      return;
    }

    CHECK(!inputSubscriber_);
    inputSubscriber_ = std::move(inputSubscriber);

    selfRef_ = shared_from_this();
    // safe to call repeatedly
    socket_->setReadCB(this);
  }

  void setOutputSubscription(yarpl::Reference<Subscription> subscription) {
    if (!subscription) {
      outputSubscription_ = nullptr;
      return;
    }

    if (isClosed()) {
      subscription->cancel();
    } else {
      outputSubscription_ = std::move(subscription);
      // no flow control at tcp level, since we can't know the size of messages
      subscription->request(kMaxRequestN);
    }
  }

  void send(std::unique_ptr<folly::IOBuf> element) {
    if (isClosed()) {
      return;
    }

    stats_->bytesWritten(element->computeChainDataLength());
    socket_->writeChain(this, std::move(element));
  }

  void close() {
    if (auto socket = std::move(socket_)) {
      socket->close();
    }
    if (auto outputSubscription = std::move(outputSubscription_)) {
      outputSubscription->cancel();
    }
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onComplete();
    }
    selfRef_ = nullptr;
  }

 private:
  void writeSuccess() noexcept override {}

  void writeErr(
      size_t bytesWritten,
      const ::folly::AsyncSocketException& ex) noexcept override {
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onError(std::make_exception_ptr(ex));
    }
    close();
  }

  void getReadBuffer(void** bufReturn, size_t* lenReturn) noexcept override {
    std::tie(*bufReturn, *lenReturn) = readBuffer_.preallocate(4096, 4096);
  }

  void readDataAvailable(size_t len) noexcept override {
    readBuffer_.postallocate(len);
    stats_->bytesRead(len);
    readBufferAvailable(readBuffer_.split(len));
  }

  void readEOF() noexcept override {
    close();
  }

  void readErr(const folly::AsyncSocketException& ex) noexcept override {
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onError(std::make_exception_ptr(ex));
    }
    close();
  }

  bool isBufferMovable() noexcept override {
    return true;
  }

  void readBufferAvailable(
      std::unique_ptr<folly::IOBuf> readBuf) noexcept override {
    CHECK(inputSubscriber_);
    inputSubscriber_->onNext(std::move(readBuf));
  }

  bool isClosed() const {
    return !socket_;
  }

  folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
  folly::AsyncSocket::UniquePtr socket_;
  const std::shared_ptr<RSocketStats> stats_;

  yarpl::Reference<Subscriber<std::unique_ptr<folly::IOBuf>>>
      inputSubscriber_;
  yarpl::Reference<Subscription> outputSubscription_;

  // self reference is used to keep the instance alive for the AsyncSocket
  // callbacks even after DuplexConnection releases references to this
  std::shared_ptr<TcpReaderWriter> selfRef_;
};

class TcpOutputSubscriber
    : public Subscriber<std::unique_ptr<folly::IOBuf>> {
 public:
  explicit TcpOutputSubscriber(
      std::shared_ptr<TcpReaderWriter> tcpReaderWriter)
      : tcpReaderWriter_(std::move(tcpReaderWriter)) {
    CHECK(tcpReaderWriter_);
  }

  void onSubscribe(
      yarpl::Reference<Subscription> subscription) noexcept override {
    CHECK(subscription);
    CHECK(tcpReaderWriter_);
    tcpReaderWriter_->setOutputSubscription(std::move(subscription));
  }

  void onNext(std::unique_ptr<folly::IOBuf> element) noexcept override {
    CHECK(tcpReaderWriter_);
    tcpReaderWriter_->send(std::move(element));
  }

  void onComplete() noexcept override {
    CHECK(tcpReaderWriter_);
    tcpReaderWriter_->setOutputSubscription(nullptr);
  }

  void onError(std::exception_ptr) noexcept override {
    onComplete();
  }

 private:
  std::shared_ptr<TcpReaderWriter> tcpReaderWriter_;
};

class TcpInputSubscription : public Subscription {
 public:
  TcpInputSubscription(
      std::shared_ptr<TcpReaderWriter> tcpReaderWriter)
      : tcpReaderWriter_(std::move(tcpReaderWriter)) {
    CHECK(tcpReaderWriter_);
  }

  void request(int64_t n) noexcept override {
    DCHECK(tcpReaderWriter_);
    DCHECK(n == kMaxRequestN) << "TcpDuplexConnection doesnt support proper flow control";
  }

  void cancel() noexcept override {
    tcpReaderWriter_->setInput(nullptr);
    tcpReaderWriter_ = nullptr;
  }

 private:
  std::shared_ptr<TcpReaderWriter> tcpReaderWriter_;
};

TcpDuplexConnection::TcpDuplexConnection(
    folly::AsyncSocket::UniquePtr&& socket,
    std::shared_ptr<RSocketStats> stats)
    : tcpReaderWriter_(
          std::make_shared<TcpReaderWriter>(std::move(socket), stats)),
      stats_(stats) {
  stats_->duplexConnectionCreated("tcp", this);
}

TcpDuplexConnection::~TcpDuplexConnection() {
  stats_->duplexConnectionClosed("tcp", this);
  tcpReaderWriter_->close();
}

yarpl::Reference<Subscriber<std::unique_ptr<folly::IOBuf>>>
TcpDuplexConnection::getOutput() {
  return yarpl::make_ref<TcpOutputSubscriber>(tcpReaderWriter_);
}

void TcpDuplexConnection::setInput(
    yarpl::Reference<Subscriber<std::unique_ptr<folly::IOBuf>>>
        inputSubscriber) {
  // we don't care if the subscriber will call request synchronously
  inputSubscriber->onSubscribe(
      yarpl::make_ref<TcpInputSubscription>(tcpReaderWriter_));
  tcpReaderWriter_->setInput(std::move(inputSubscriber));
}

} // rsocket
