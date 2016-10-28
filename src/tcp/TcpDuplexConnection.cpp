// Copyright 2004-present Facebook. All Rights Reserved.

#include "TcpDuplexConnection.h"
#include <folly/ExceptionWrapper.h>
#include <folly/SocketAddress.h>
#include <folly/io/IOBufQueue.h>
#include <folly/io/async/AsyncTransport.h>
#include <glog/logging.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "src/SubscriberBase.h"
#include "src/SubscriptionBase.h"

namespace reactivesocket {
using namespace ::folly;

class TcpReaderWriter : public ::folly::AsyncTransportWrapper::WriteCallback,
                        public ::folly::AsyncTransportWrapper::ReadCallback,
                        public SubscriptionBase,
                        public SubscriberBaseT<std::unique_ptr<folly::IOBuf>> {
 public:
  explicit TcpReaderWriter(
      folly::AsyncSocket::UniquePtr&& socket,
      Stats& stats = Stats::noop())
      : socket_(std::move(socket)), stats_(stats) {}

  ~TcpReaderWriter() {
    socket_->close();
  }

  void setInput(
      std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
          inputSubscriber) {
    CHECK(!inputSubscriber_);
    inputSubscriber_.reset(std::move(inputSubscriber));
    inputSubscriber_.onSubscribe(SubscriptionBase::shared_from_this());

    socket_->setReadCB(this);
  }

 private:
  void onSubscribeImpl(std::shared_ptr<Subscription> subscription) override {
    // no flow control at tcp level, since we can't know the size of messages
    subscription->request(std::numeric_limits<size_t>::max());
  }

  void onNextImpl(std::unique_ptr<folly::IOBuf> element) override {
    send(std::move(element));
  }

  void onCompleteImpl() override {
    closeFromWriter();
  }

  void onErrorImpl(folly::exception_wrapper ex) override {
    closeFromWriter();
  }

  void requestImpl(size_t n) override {
    // ignored for now, currently flow control is only at higher layers
  }

  void cancelImpl() override {
    closeFromReader();
  }

  void send(std::unique_ptr<folly::IOBuf> element) {
    stats_.bytesWritten(element->computeChainDataLength());
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
    if (inputSubscriber_) {
      inputSubscriber_.onError(ex);
    }
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
    inputSubscriber_.onComplete();
  }

  void readErr(const folly::AsyncSocketException& ex) noexcept override {
    inputSubscriber_.onError(ex);
  }

  bool isBufferMovable() noexcept override {
    return true;
  }

  void readBufferAvailable(
      std::unique_ptr<folly::IOBuf> readBuf) noexcept override {
    inputSubscriber_.onNext(std::move(readBuf));
  }

  folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
  folly::AsyncSocket::UniquePtr socket_;
  Stats& stats_;

  SubscriberPtr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      inputSubscriber_;
};

TcpDuplexConnection::TcpDuplexConnection(
    folly::AsyncSocket::UniquePtr&& socket,
    Stats& stats)
    : tcpReaderWriter_(
          std::make_shared<TcpReaderWriter>(std::move(socket), stats)),
      stats_(stats) {
  stats_.connectionCreated("tcp", this);
}

TcpDuplexConnection::~TcpDuplexConnection() {
  stats_.connectionClosed("tcp", this);
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
