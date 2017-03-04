// Copyright 2004-present Facebook. All Rights Reserved.

#include "TcpDuplexConnection.h"
#include <folly/ExceptionWrapper.h>
#include "src/SubscriberBase.h"
#include "src/SubscriptionBase.h"

namespace reactivesocket {
using namespace ::folly;

class TcpReaderWriter : public ::folly::AsyncTransportWrapper::WriteCallback,
                        public ::folly::AsyncTransportWrapper::ReadCallback {
 public:
  explicit TcpReaderWriter(
      folly::AsyncSocket::UniquePtr&& socket,
      Stats& stats = Stats::noop())
      : socket_(std::move(socket)), stats_(stats) {}

  ~TcpReaderWriter() {
    CHECK(isClosed());
    DCHECK(!inputSubscriber_);
  }

  void setInput(
      std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
          inputSubscriber) {
    if (isClosed()) {
      inputSubscriber->onComplete();
      return;
    }

    CHECK(!inputSubscriber_);
    inputSubscriber_ = std::move(inputSubscriber);

    // safe to call repeatedly
    socket_->setReadCB(this);
  }

  void setOutputSubscription(std::shared_ptr<Subscription> subscription) {
    if (isClosed()) {
      subscription->cancel();
    } else {
      // no flow control at tcp level, since we can't know the size of messages
      subscription->request(std::numeric_limits<size_t>::max());
      outputSubscription_ = std::move(subscription);
    }
  }

  void send(std::unique_ptr<folly::IOBuf> element) {
    if (isClosed()) {
      return;
    }

    stats_.bytesWritten(element->computeChainDataLength());
    socket_->writeChain(this, std::move(element));
  }

  void closeFromWriter() {
    if (isClosed()) {
      return;
    }

    socket_->close();
  }

  void closeFromReader() {
    closeFromWriter();
  }

 private:
  void writeSuccess() noexcept override {}

  void writeErr(
      size_t bytesWritten,
      const ::folly::AsyncSocketException& ex) noexcept override {
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onError(ex);
    }
    close();
  }

  void getReadBuffer(void** bufReturn, size_t* lenReturn) noexcept override {
    std::tie(*bufReturn, *lenReturn) = readBuffer_.preallocate(4096, 4096);
  }

  void readDataAvailable(size_t len) noexcept override {
    readBuffer_.postallocate(len);
    stats_.bytesRead(len);

    if (inputSubscriber_) {
      readBufferAvailable(readBuffer_.split(len));
    }
  }

  void readEOF() noexcept override {
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onComplete();
    }
    close();
  }

  void readErr(const folly::AsyncSocketException& ex) noexcept override {
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onError(ex);
    }
    close();
  }

  bool isBufferMovable() noexcept override {
    return true;
  }

  void readBufferAvailable(
      std::unique_ptr<folly::IOBuf> readBuf) noexcept override {
    inputSubscriber_->onNext(std::move(readBuf));
  }

  bool isClosed() const {
    return !socket_;
  }

  void close() {
    if (auto socket = std::move(socket_)) {
      socket->close();
    }
    if (auto outputSubscription = std::move(outputSubscription_)) {
      outputSubscription->cancel();
    }
  }

  folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
  folly::AsyncSocket::UniquePtr socket_;
  Stats& stats_;

  std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      inputSubscriber_;
  std::shared_ptr<reactivesocket::Subscription> outputSubscription_;
};

class TcpOutputSubscriber
    : public SubscriberBaseT<std::unique_ptr<folly::IOBuf>> {
 public:
  TcpOutputSubscriber(
      std::shared_ptr<TcpReaderWriter> tcpReaderWriter,
      folly::Executor& executor)
      : ExecutorBase(executor), tcpReaderWriter_(std::move(tcpReaderWriter)) {}

  void onSubscribeImpl(
      std::shared_ptr<Subscription> subscription) noexcept override {
    if (tcpReaderWriter_) {
      // no flow control at tcp level, since we can't know the size of messages
      subscription->request(std::numeric_limits<size_t>::max());
      tcpReaderWriter_->setOutputSubscription(std::move(subscription));
    } else {
      LOG(ERROR) << "trying to resubscribe on a closed subscriber";
      subscription->cancel();
    }
  }

  void onNextImpl(std::unique_ptr<folly::IOBuf> element) noexcept override {
    CHECK(tcpReaderWriter_);
    tcpReaderWriter_->send(std::move(element));
  }

  void onCompleteImpl() noexcept override {
    CHECK(tcpReaderWriter_);
    auto tcpReaderWriter = std::move(tcpReaderWriter_);
    tcpReaderWriter->closeFromWriter();
  }

  void onErrorImpl(folly::exception_wrapper ex) noexcept override {
    onCompleteImpl();
  }

 private:
  std::shared_ptr<TcpReaderWriter> tcpReaderWriter_;
};

class TcpInputSubscription : public SubscriptionBase {
 public:
  TcpInputSubscription(
      std::shared_ptr<TcpReaderWriter> tcpReaderWriter,
      folly::Executor& executor)
      : ExecutorBase(executor), tcpReaderWriter_(std::move(tcpReaderWriter)) {
    CHECK(tcpReaderWriter_);
  }

  void requestImpl(size_t n) noexcept override {
    // TcpDuplexConnection doesnt support propper flow control
  }

  void cancelImpl() noexcept override {
    tcpReaderWriter_->closeFromReader();
  }

 private:
  std::shared_ptr<TcpReaderWriter> tcpReaderWriter_;
};

TcpDuplexConnection::TcpDuplexConnection(
    folly::AsyncSocket::UniquePtr&& socket,
    folly::Executor& executor,
    Stats& stats)
    : tcpReaderWriter_(
          std::make_shared<TcpReaderWriter>(std::move(socket), stats)),
      stats_(stats),
      executor_(executor) {
  stats_.duplexConnectionCreated("tcp", this);
}

TcpDuplexConnection::~TcpDuplexConnection() {
  stats_.duplexConnectionClosed("tcp", this);
}

std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
TcpDuplexConnection::getOutput() {
  return std::make_shared<TcpOutputSubscriber>(tcpReaderWriter_, executor_);
}

void TcpDuplexConnection::setInput(
    std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
        inputSubscriber) {
  inputSubscriber->onSubscribe(
      std::make_shared<TcpInputSubscription>(tcpReaderWriter_, executor_));
  tcpReaderWriter_->setInput(std::move(inputSubscriber));
}

} // reactivesocket
