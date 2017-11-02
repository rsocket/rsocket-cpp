// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/transports/tcp/TcpDuplexConnection.h"

#include <folly/Baton.h>
#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/ProducerConsumerQueue.h>
#include <folly/io/IOBufQueue.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/system/ThreadName.h>

#include "rsocket/internal/Common.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

using namespace yarpl::flowable;

class TcpReaderWriter : public folly::AsyncTransportWrapper::WriteCallback,
                        public folly::AsyncTransportWrapper::ReadCallback {
  friend void intrusive_ptr_add_ref(TcpReaderWriter* x);
  friend void intrusive_ptr_release(TcpReaderWriter* x);

 private:
  std::atomic<int> refCount_{0};
};

class SimpleTcpReaderWriter : public TcpReaderWriter {
 public:
  explicit SimpleTcpReaderWriter(
      folly::AsyncTransportWrapper::UniquePtr&& socket,
      std::shared_ptr<RSocketStats> stats)
      : socket_(std::move(socket)), stats_(std::move(stats)) {}

  ~SimpleTcpReaderWriter() {
    CHECK(isClosed());
    DCHECK(!inputSubscriber_);
  }

  folly::AsyncTransportWrapper* getTransport() {
    return socket_.get();
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

    if (!socket_->getReadCallback()) {
      // The AsyncSocket will hold a reference to this instance until it calls
      // readEOF or readErr.
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
    closeErr(folly::exception_wrapper{exn});
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
  folly::AsyncTransportWrapper::UniquePtr socket_;
  const std::shared_ptr<RSocketStats> stats_;

  yarpl::Reference<DuplexConnection::Subscriber> inputSubscriber_;
  yarpl::Reference<Subscription> outputSubscription_;
};

class BatchingTcpReaderWriter : public TcpReaderWriter {
 public:
  explicit BatchingTcpReaderWriter(
      folly::AsyncTransportWrapper::UniquePtr&& socket,
      std::shared_ptr<RSocketStats> stats)
      : socket_(std::move(socket)),
        stats_(std::move(stats)),
        sendToEvb_(socket_->getEventBase()),
        bufsToSend_(1024),
        batchingThread_(
            folly::sformat("Batch_{}", *folly::getCurrentThreadName())) {
    socket_->detachEventBase();
    batchingEvb_ = batchingThread_.getEventBase();
    batchingEvb_->runInEventBaseThreadAndWait(
        [&] { socket_->attachEventBase(batchingEvb_); });

    DCHECK(sendToEvb_);
    DCHECK(sendToEvb_->isInEventBaseThread());

    // the batching thread has a reference to this, which won't be
    // released until it calls socket_->close()
    intrusive_ptr_add_ref(this);
  }

 private:
  // Queues a write job on the I/O thread's eventbase, batchingEvb_,
  // if there is not a write job for this reader/writer scheduled
  void runBatchingEvb() {
    if (this->batchRunPending_) {
      return;
    }
    this->batchRunPending_ = true;
    batchingEvb_->runInEventBaseThread([this] {
      int read_bufs = 0;
      folly::IOBuf* bufptr{nullptr};

      if (bufsToSend_.read(bufptr)) {
        std::unique_ptr<folly::IOBuf> buf{bufptr};

        read_bufs++;
        while (bufsToSend_.read(bufptr)) {
          std::unique_ptr<folly::IOBuf> nextBuf{bufptr};
          read_bufs++;
          buf->prependChain(std::move(nextBuf));
        }

        // if the other thread was blocking because we used the whole buffer,
        // let it go now
        sentIOBufs_.post();

        // TODO: log read_bufs somewhere, to track how effective microbatching
        // is?

        intrusive_ptr_add_ref(this);
        socket_->writeChain(this, std::move(buf));
      }

      this->batchRunPending_ = false;
    });
  }

 public:
  ~BatchingTcpReaderWriter() {
    // must destroy batchingThread_ in sendtoEvb_'s thread, as that's where
    // it was constructed
    CHECK(sendToEvb_->isInEventBaseThread());
    CHECK(isClosed());
    DCHECK(!inputSubscriber_);
  }

  folly::AsyncTransportWrapper* getTransport() {
    CHECK(sendToEvb_->isInEventBaseThread());
    return socket_.get();
  }

  void setInput(
      yarpl::Reference<DuplexConnection::Subscriber> inputSubscriber) {
    CHECK(sendToEvb_->isInEventBaseThread());

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

    if (!socket_->getReadCallback()) {
      // The AsyncSocket will hold a reference to this instance until it calls
      // readEOF or readErr.
      batchingEvb_->runInEventBaseThreadAndWait([this] {
        intrusive_ptr_add_ref(this);
        socket_->setReadCB(this);
      });
    }
  }

  void setOutputSubscription(yarpl::Reference<Subscription> subscription) {
    CHECK(sendToEvb_->isInEventBaseThread());

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

  // called from sendtoEvb
  void send(std::unique_ptr<folly::IOBuf> element) {
    CHECK(sendToEvb_->isInEventBaseThread());

    if (isClosed()) {
      return;
    }

    if (stats_) {
      stats_->bytesWritten(element->computeChainDataLength());
    }
    // now AsyncSocket will hold a reference to this instance as a writer until
    // they call writeComplete or writeErr
    // intrusive_ptr_add_ref(this);
    // socket_->writeChain(this, std::move(element));
    auto bufptr = element.release();
    while (!bufsToSend_.write(bufptr)) {
      runBatchingEvb();
      sentIOBufs_.timed_wait(std::chrono::milliseconds(100));
      sentIOBufs_.reset();
    }
    runBatchingEvb();
  }

  void close() {
    CHECK(sendToEvb_->isInEventBaseThread());

    closeSocketRemotely();

    if (auto outputSubscription = std::move(outputSubscription_)) {
      outputSubscription->cancel();
    }
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onComplete();
    }
  }

  void closeErr(folly::exception_wrapper ew) {
    CHECK(sendToEvb_->isInEventBaseThread());

    closeSocketRemotely();

    if (auto subscription = std::move(outputSubscription_)) {
      subscription->cancel();
    }
    if (auto subscriber = std::move(inputSubscriber_)) {
      subscriber->onError(std::move(ew));
    }
  }

  void closeSocketRemotely() {
    CHECK(sendToEvb_->isInEventBaseThread());

    batchingEvb_->runInEventBaseThread([&] {
      if (auto socket = std::move(socket_)) {
        socket->close();

        // release the reference held by this thread (sibling call to
        // intrusive_ptr_add_ref is in
        // the constructor) in the sendToEvb_ thread in case the destructor is
        // triggered
        sendToEvb_->runInEventBaseThread(
            [this] { intrusive_ptr_release(this); });
      }
    });
  }

 private:
  bool isClosed() const {
    return !socket_;
  }

  void writeSuccess() noexcept override {
    CHECK(batchingEvb_->isInEventBaseThread());
    intrusive_ptr_release(this);
  }

  void writeErr(
      size_t,
      const folly::AsyncSocketException& exn) noexcept override {
    CHECK(batchingEvb_->isInEventBaseThread());

    sendToEvb_->runInEventBaseThread([ this, exn = std::move(exn) ]() mutable {
      closeErr(folly::exception_wrapper{exn});
      intrusive_ptr_release(this);
    });
  }

  void getReadBuffer(void** bufReturn, size_t* lenReturn) noexcept override {
    CHECK(batchingEvb_->isInEventBaseThread());

    std::tie(*bufReturn, *lenReturn) = readBuffer_.preallocate(4096, 4096);
  }

  void readDataAvailable(size_t len) noexcept override {
    CHECK(batchingEvb_->isInEventBaseThread());

    readBuffer_.postallocate(len);
    if (stats_) {
      stats_->bytesRead(len);
    }

    if (inputSubscriber_) {
      readBufferAvailable(readBuffer_.split(len));
    }
  }

  void readEOF() noexcept override {
    CHECK(batchingEvb_->isInEventBaseThread());
    sendToEvb_->runInEventBaseThread([this] {
      close();
      intrusive_ptr_release(this);
    });
  }

  void readErr(const folly::AsyncSocketException& exn) noexcept override {
    CHECK(batchingEvb_->isInEventBaseThread());
    sendToEvb_->runInEventBaseThread([ this, exn = std::move(exn) ] {
      closeErr(exn);
      intrusive_ptr_release(this);
    });
  }

  bool isBufferMovable() noexcept override {
    CHECK(batchingEvb_->isInEventBaseThread());
    return true;
  }

  void readBufferAvailable(
      std::unique_ptr<folly::IOBuf> readBuf) noexcept override {
    CHECK(batchingEvb_->isInEventBaseThread());
    CHECK(inputSubscriber_);

    sendToEvb_->runInEventBaseThread(
        [ insub = inputSubscriber_, buf = std::move(readBuf) ]() mutable {
          insub->onNext(std::move(buf));
        });
  }

  folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
  folly::AsyncTransportWrapper::UniquePtr socket_;
  const std::shared_ptr<RSocketStats> stats_;

  yarpl::Reference<DuplexConnection::Subscriber> inputSubscriber_;
  yarpl::Reference<Subscription> outputSubscription_;

  // eventbase running in the batching thread
  folly::EventBase* batchingEvb_{nullptr};
  // thread that constructs us
  folly::EventBase* sendToEvb_{nullptr};

  folly::ProducerConsumerQueue<folly::IOBuf*> bufsToSend_;
  folly::ScopedEventBaseThread batchingThread_;
  folly::Baton<std::atomic, false> sentIOBufs_;

  std::atomic<bool> batchRunPending_{};
};

void intrusive_ptr_add_ref(TcpReaderWriter* x);
void intrusive_ptr_release(TcpReaderWriter* x);

inline void intrusive_ptr_add_ref(TcpReaderWriter* x) {
  ++x->refCount_;
}

inline void intrusive_ptr_release(TcpReaderWriter* x) {
  if (--x->refCount_ == 0) {
    delete x;
  }
}

namespace {

template <typename ReaderWriter>
class TcpOutputSubscriber : public DuplexConnection::Subscriber {
 public:
  explicit TcpOutputSubscriber(
      boost::intrusive_ptr<ReaderWriter> tcpReaderWriter)
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
  boost::intrusive_ptr<ReaderWriter> tcpReaderWriter_;
};

template <typename ReaderWriter>
class TcpInputSubscription : public Subscription {
 public:
  explicit TcpInputSubscription(
      boost::intrusive_ptr<ReaderWriter> tcpReaderWriter)
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
  boost::intrusive_ptr<ReaderWriter> tcpReaderWriter_;
};
}

template <typename ReaderWriter>
GenericTcpDuplexConnection<ReaderWriter>::GenericTcpDuplexConnection(
    folly::AsyncTransportWrapper::UniquePtr&& socket,
    std::shared_ptr<RSocketStats> stats)
    : tcpReaderWriter_(new ReaderWriter(std::move(socket), stats)),
      stats_(stats) {
  if (stats_) {
    stats_->duplexConnectionCreated("tcp", this);
  }
}

template <typename ReaderWriter>
GenericTcpDuplexConnection<ReaderWriter>::~GenericTcpDuplexConnection() {
  if (stats_) {
    stats_->duplexConnectionClosed("tcp", this);
  }
  tcpReaderWriter_->close();
}

template <typename ReaderWriter>
folly::AsyncTransportWrapper*
GenericTcpDuplexConnection<ReaderWriter>::getTransport() {
  return tcpReaderWriter_ ? tcpReaderWriter_->getTransport() : nullptr;
}

template <typename ReaderWriter>
yarpl::Reference<DuplexConnection::Subscriber>
GenericTcpDuplexConnection<ReaderWriter>::getOutput() {
  return yarpl::make_ref<TcpOutputSubscriber<ReaderWriter>>(tcpReaderWriter_);
}

template <typename ReaderWriter>
void GenericTcpDuplexConnection<ReaderWriter>::setInput(
    yarpl::Reference<DuplexConnection::Subscriber> inputSubscriber) {
  // we don't care if the subscriber will call request synchronously
  inputSubscriber->onSubscribe(
      yarpl::make_ref<TcpInputSubscription<ReaderWriter>>(tcpReaderWriter_));
  tcpReaderWriter_->setInput(std::move(inputSubscriber));
}

// explicltly instantiate
template class GenericTcpDuplexConnection<SimpleTcpReaderWriter>;
template class GenericTcpDuplexConnection<BatchingTcpReaderWriter>;
}
