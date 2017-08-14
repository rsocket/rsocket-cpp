// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Benchmark.h>
#include <folly/io/IOBufQueue.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#define PORT (35437)

using namespace folly;

namespace {

class TcpReader : public ::folly::AsyncTransportWrapper::ReadCallback {
 public:
  TcpReader(
      folly::AsyncSocket::UniquePtr&& socket,
      EventBase& eventBase,
      size_t loadSize,
      size_t recvBufferLength)
      : socket_(std::move(socket)),
        eventBase_(eventBase),
        loadSize_(loadSize),
        recvBufferLength_(recvBufferLength) {
    socket_->setReadCB(this);
  }

 private:
  void getReadBuffer(void** bufReturn, size_t* lenReturn) noexcept override {
    std::tie(*bufReturn, *lenReturn) =
        readBuffer_.preallocate(recvBufferLength_, recvBufferLength_);
  }

  void readDataAvailable(size_t len) noexcept override {
    readBuffer_.postallocate(len);
    auto readData = readBuffer_.split(len);

    receivedLength_ += readData->computeChainDataLength();
    if (receivedLength_ >= loadSize_) {
      close();
    }
  }

  void readBufferAvailable(
      std::unique_ptr<folly::IOBuf> readBuf) noexcept override {
    receivedLength_ += readBuf->computeChainDataLength();
    if (receivedLength_ >= loadSize_) {
      close();
    }
  }

  void readEOF() noexcept override {
    close();
  }

  void readErr(const folly::AsyncSocketException& exn) noexcept override {
    LOG(ERROR) << exn.what();
    close();
  }

  bool isBufferMovable() noexcept override {
    return true;
  }

  void close() {
    if (socket_) {
      auto socket = std::move(socket_);
      socket->close();
      eventBase_.terminateLoopSoon();
      delete this;
    }
  }

  folly::AsyncSocket::UniquePtr socket_;
  folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
  EventBase& eventBase_;
  const size_t loadSize_;
  const size_t recvBufferLength_;
  size_t receivedLength_{0};
};

class ServerAcceptCallback : public AsyncServerSocket::AcceptCallback {
 public:
  ServerAcceptCallback(
      EventBase& eventBase,
      size_t loadSize,
      size_t recvBufferLength)
      : eventBase_(eventBase),
        loadSize_(loadSize),
        recvBufferLength_(recvBufferLength) {}

  void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override {
    auto socket =
        folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));

    TcpReader* reader = new TcpReader(
        std::move(socket), eventBase_, loadSize_, recvBufferLength_);
  }

  void acceptError(const std::exception& ex) noexcept override {
    LOG(ERROR) << "acceptError" << ex.what() << std::endl;
    eventBase_.terminateLoopSoon();
  }

 private:
  EventBase& eventBase_;
  const size_t loadSize_;
  const size_t recvBufferLength_;
};

class TcpWriter : public ::folly::AsyncTransportWrapper::WriteCallback {
 public:
  void startWriting(AsyncSocket& socket, size_t loadSize, size_t messageSize) {
    size_t bytesSent{0};

    while (!closed_ && bytesSent < loadSize) {
      auto data = IOBuf::copyBuffer(std::string(messageSize, 'a'));
      socket.writeChain(this, std::move(data));
      bytesSent += messageSize;
    }
  }

 private:
  void writeSuccess() noexcept override {
  }

  void writeErr(
      size_t,
      const folly::AsyncSocketException& exn) noexcept override {
    LOG(ERROR) << "writeError: " << exn.what();
    closed_ = true;
  }

  bool closed_{false};
};

class ClientConnectCallback : public AsyncSocket::ConnectCallback {
 public:
  ClientConnectCallback(EventBase& eventBase, size_t loadSize, size_t msgLength)
      : eventBase_(eventBase), loadSize_(loadSize), msgLength_(msgLength) {}

  void connect() {
    eventBase_.runInEventBaseThread([this] {
      socket_.reset(new AsyncSocket(&eventBase_));
      SocketAddress clientAaddr("::", PORT);
      socket_->connect(this, clientAaddr);
    });
  }

 private:
  void connectSuccess() noexcept override {
    TcpWriter writer;
    writer.startWriting(*socket_, loadSize_, msgLength_);
    socket_->close();
    delete this;
  }

  void connectErr(const AsyncSocketException& ex) noexcept override {
    LOG(ERROR) << "connectErr: " << ex.what() << " " << ex.getType();
    delete this;
  }

  AsyncSocket::UniquePtr socket_;
  EventBase& eventBase_;
  const size_t loadSize_;
  const size_t msgLength_;
};
}

static void BM_Baseline_AsyncSocket_SendReceive(
    size_t loadSize,
    size_t msgLength,
    size_t recvLength) {
  EventBase serverEventBase;
  auto serverSocket = AsyncServerSocket::newSocket(&serverEventBase);

  ServerAcceptCallback serverCallback(serverEventBase, loadSize, recvLength);

  SocketAddress addr("::", PORT);

  serverSocket->setReusePortEnabled(true);
  serverSocket->bind(addr);
  serverSocket->addAcceptCallback(&serverCallback, &serverEventBase);
  serverSocket->listen(1);
  serverSocket->startAccepting();

  ScopedEventBaseThread clientThread;
  auto* clientCallback = new ClientConnectCallback(
      *clientThread.getEventBase(), loadSize, msgLength);
  clientCallback->connect();

  serverEventBase.loopForever();
}

void BM_Baseline_AsyncSocket_Throughput_100MB_s40B_r1024B(unsigned, unsigned) {
  constexpr size_t loadSizeB = 100 * 1024 * 1024;
  constexpr size_t sendSizeB = 40;
  constexpr size_t receiveSizeB = 1024;
  BM_Baseline_AsyncSocket_SendReceive(loadSizeB, sendSizeB, receiveSizeB);
}
void BM_Baseline_AsyncSocket_Throughput_100MB_s40B_r4096B(unsigned, unsigned) {
  constexpr size_t loadSizeB = 100 * 1024 * 1024;
  constexpr size_t sendSizeB = 40;
  constexpr size_t receiveSizeB = 4096;
  BM_Baseline_AsyncSocket_SendReceive(loadSizeB, sendSizeB, receiveSizeB);
}
void BM_Baseline_AsyncSocket_Throughput_100MB_s80B_r4096B(unsigned, unsigned) {
  constexpr size_t loadSizeB = 100 * 1024 * 1024;
  constexpr size_t sendSizeB = 80;
  constexpr size_t receiveSizeB = 4096;
  BM_Baseline_AsyncSocket_SendReceive(loadSizeB, sendSizeB, receiveSizeB);
}
void BM_Baseline_AsyncSocket_Throughput_100MB_s4096B_r4096B(
    unsigned,
    unsigned) {
  constexpr size_t loadSizeB = 100 * 1024 * 1024;
  constexpr size_t sendSizeB = 4096;
  constexpr size_t receiveSizeB = 4096;
  BM_Baseline_AsyncSocket_SendReceive(loadSizeB, sendSizeB, receiveSizeB);
}

BENCHMARK_PARAM(BM_Baseline_AsyncSocket_Throughput_100MB_s40B_r1024B, 1);
BENCHMARK_PARAM(BM_Baseline_AsyncSocket_Throughput_100MB_s40B_r4096B, 1);
BENCHMARK_PARAM(BM_Baseline_AsyncSocket_Throughput_100MB_s80B_r4096B, 1);
BENCHMARK_PARAM(BM_Baseline_AsyncSocket_Throughput_100MB_s4096B_r4096B, 1);

void BM_Baseline_AsyncSocket_Latency_1M_msgs_32B(unsigned, unsigned) {
  constexpr size_t messageSizeB = 32;
  constexpr size_t loadSizeB = 1000000 * messageSizeB;
  BM_Baseline_AsyncSocket_SendReceive(loadSizeB, messageSizeB, messageSizeB);
}
void BM_Baseline_AsyncSocket_Latency_1M_msgs_128B(unsigned, unsigned) {
  constexpr size_t messageSizeB = 128;
  constexpr size_t loadSizeB = 1000000 * messageSizeB;
  BM_Baseline_AsyncSocket_SendReceive(loadSizeB, messageSizeB, messageSizeB);
}
void BM_Baseline_AsyncSocket_Latency_1M_msgs_4kB(unsigned, unsigned) {
  constexpr size_t messageSizeB = 4096;
  constexpr size_t loadSizeB = 1000000 * messageSizeB;
  BM_Baseline_AsyncSocket_SendReceive(loadSizeB, messageSizeB, messageSizeB);
}

BENCHMARK_PARAM(BM_Baseline_AsyncSocket_Latency_1M_msgs_32B, 1);
BENCHMARK_PARAM(BM_Baseline_AsyncSocket_Latency_1M_msgs_128B, 1);
BENCHMARK_PARAM(BM_Baseline_AsyncSocket_Latency_1M_msgs_4kB, 1);
