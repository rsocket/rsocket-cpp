// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/transports/tcp/TcpDuplexConnection.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBufQueue.h>

#include "rsocket/internal/Common.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

using namespace yarpl::flowable;

class TcpReaderWriter : public folly::AsyncTransportWrapper::WriteCallback,
                        public folly::AsyncTransportWrapper::ReadCallback {

  friend void intrusive_ptr_add_ref(TcpReaderWriter* x);
  friend void intrusive_ptr_release(TcpReaderWriter* x);

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
      yarpl::Reference<DuplexConnection::Subscriber> inputSubscriber) {
    if (inputSubscriber && isClosed()) {
      inputSubscriber->onComplete();
      return;
    }

    if (!inputSubscriber) {
      inputSubscriber_ = nullptr;
      return;
    }

    CHECK(!inputSubscriber_);
    inputSubscriber_ = std::move(inputSubscriber);

    if (!readerSet_) {
      readerSet_ = true;
      // now AsyncSocket will hold a reference to this instance until
      // they call readEOF or readErr
      intrusive_ptr_add_ref(this);
      socket_->setReadCB(this);
    }
  }

  void setOutputSubscription(yarpl::Reference<Subscription> subscription) {
    if (!subscription) {
      outputSubscription_ = nullptr;
      return;
    }

    if (isClosed()) {
      subscription->cancel();
      return;
    }

    // No flow control at TCP level for output
    // The AsyncSocket will accept all send calls
    subscription->request(std::numeric_limits<int64_t>::max());
    outputSubscription_ = std::move(subscription);
  }

  void send(std::unique_ptr<folly::IOBuf> element) {
    if (isClosed()) {
      return;
    }

    if (stats_) {
      stats_->bytesWritten(element->computeChainDataLength());
    }
    // now AsyncSocket will hold a reference to this instance as a writer until
    // they call writeComplete or writeErr
    intrusive_ptr_add_ref(this);
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
  }

  void closeErr(folly::exception_wrapper ew) {
    if (auto socket = std::move(socket_)) {
      socket->close();
    }
    if (auto subscription = std::move(outputSubscription_)) {
      subscription->cancel();
    }
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onError(std::move(ew));
    }
  }

 private:
  bool isClosed() const {
    return !socket_;
  }

  void writeSuccess() noexcept override {
    intrusive_ptr_release(this);
  }

  void writeErr(
      size_t,
      const folly::AsyncSocketException& exn) noexcept override {
    closeErr(exn);
    intrusive_ptr_release(this);
  }

  void getReadBuffer(void** bufReturn, size_t* lenReturn) noexcept override {
    std::tie(*bufReturn, *lenReturn) = readBuffer_.preallocate(4096, 4096);
  }

  void readDataAvailable(size_t len) noexcept override {
    readBuffer_.postallocate(len);
    if (stats_) {
      stats_->bytesRead(len);
    }

    if (inputSubscriber_) {
      readBufferAvailable(readBuffer_.split(len));
    }
  }

  void readEOF() noexcept override {
    close();
    intrusive_ptr_release(this);
  }

  void readErr(const folly::AsyncSocketException& exn) noexcept override {
    closeErr(exn);
    intrusive_ptr_release(this);
  }

  bool isBufferMovable() noexcept override {
    return true;
  }

  void readBufferAvailable(
      std::unique_ptr<folly::IOBuf> readBuf) noexcept override {
    CHECK(inputSubscriber_);
    inputSubscriber_->onNext(std::move(readBuf));
  }

  folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
  folly::AsyncSocket::UniquePtr socket_;
  const std::shared_ptr<RSocketStats> stats_;

  yarpl::Reference<DuplexConnection::Subscriber> inputSubscriber_;
  yarpl::Reference<Subscription> outputSubscription_;
  bool readerSet_{false};
  int refCount_{0};
};

void intrusive_ptr_add_ref(TcpReaderWriter* x);
void intrusive_ptr_release(TcpReaderWriter* x);

inline void intrusive_ptr_add_ref(TcpReaderWriter* x){
  ++x->refCount_;
}

inline void intrusive_ptr_release(TcpReaderWriter* x){
  if(--x->refCount_ == 0)
    delete x;
}

namespace {

class TcpOutputSubscriber : public DuplexConnection::Subscriber {
 public:
  explicit TcpOutputSubscriber(boost::intrusive_ptr<TcpReaderWriter> tcpReaderWriter)
      : tcpReaderWriter_(std::move(tcpReaderWriter)) {
    CHECK(tcpReaderWriter_);
  }

  void onSubscribe(yarpl::Reference<Subscription> subscription) override {
    CHECK(subscription);
    CHECK(tcpReaderWriter_);
    tcpReaderWriter_->setOutputSubscription(std::move(subscription));
  }

  void onNext(std::unique_ptr<folly::IOBuf> element) override {
    CHECK(tcpReaderWriter_);
    tcpReaderWriter_->send(std::move(element));
  }

  void onComplete() override {
    CHECK(tcpReaderWriter_);
    tcpReaderWriter_->setOutputSubscription(nullptr);
  }

  void onError(folly::exception_wrapper) override {
    CHECK(tcpReaderWriter_);
    tcpReaderWriter_->setOutputSubscription(nullptr);
  }

 private:
  boost::intrusive_ptr<TcpReaderWriter> tcpReaderWriter_;
};

class TcpInputSubscription : public Subscription {
 public:
  explicit TcpInputSubscription(
      boost::intrusive_ptr<TcpReaderWriter> tcpReaderWriter)
      : tcpReaderWriter_(std::move(tcpReaderWriter)) {
    CHECK(tcpReaderWriter_);
  }

  void request(int64_t n) noexcept override {
    DCHECK(tcpReaderWriter_);
    DCHECK_EQ(n, std::numeric_limits<int64_t>::max())
        << "TcpDuplexConnection doesnt support proper flow control";
  }

  void cancel() noexcept override {
    tcpReaderWriter_->setInput(nullptr);
    tcpReaderWriter_ = nullptr;
  }

 private:
  boost::intrusive_ptr<TcpReaderWriter> tcpReaderWriter_;
};
}

TcpDuplexConnection::TcpDuplexConnection(
    folly::AsyncSocket::UniquePtr&& socket,
    std::shared_ptr<RSocketStats> stats)
    : tcpReaderWriter_(
          new TcpReaderWriter(std::move(socket), stats)),
      stats_(stats) {
  if (stats_) {
    stats_->duplexConnectionCreated("tcp", this);
  }
}

TcpDuplexConnection::~TcpDuplexConnection() {
  if (stats_) {
    stats_->duplexConnectionClosed("tcp", this);
  }
  tcpReaderWriter_->close();
}

yarpl::Reference<DuplexConnection::Subscriber>
TcpDuplexConnection::getOutput() {
  return yarpl::make_ref<TcpOutputSubscriber>(tcpReaderWriter_);
}

void TcpDuplexConnection::setInput(
    yarpl::Reference<DuplexConnection::Subscriber> inputSubscriber) {
  // we don't care if the subscriber will call request synchronously
  inputSubscriber->onSubscribe(
      yarpl::make_ref<TcpInputSubscription>(tcpReaderWriter_));
  tcpReaderWriter_->setInput(std::move(inputSubscriber));
}
}
