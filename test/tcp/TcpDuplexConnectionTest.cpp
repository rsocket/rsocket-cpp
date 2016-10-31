// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/test/MockAsyncSocket.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/tcp/TcpDuplexConnection.h"
#include "test/streams/Mocks.h"

using namespace ::testing;
using namespace ::reactivesocket;
using namespace ::reactivestreams;

using folly::AsyncSocket;
using folly::test::MockAsyncSocket;
using folly::EventBase;

class MockAsyncSocketWithWrite : public MockAsyncSocket {
 public:
  explicit MockAsyncSocketWithWrite(EventBase* base) : MockAsyncSocket(base) {
  }
  MOCK_METHOD3(writeChain_, void(WriteCallback*, std::unique_ptr<folly::IOBuf>&, folly::WriteFlags));
  void writeChain(WriteCallback* callback,
                  std::unique_ptr<folly::IOBuf>&& buf,
                  folly::WriteFlags flags = folly::WriteFlags::NONE) override {
    writeChain_(callback, buf, flags);
  }
};

TEST(TcpDuplexConnectionTest, SetInput) {
  EventBase eventBase_;
  StrictMock<MockAsyncSocket> *socket_ = new StrictMock<MockAsyncSocket>(&eventBase_);
  EXPECT_CALL(*socket_, setReadCB(_));
  EXPECT_CALL(*socket_, closeNow()).Times(AtLeast(1));
  std::unique_ptr<TcpDuplexConnection> connection =
      folly::make_unique<TcpDuplexConnection>(AsyncSocket::UniquePtr(socket_));

  MockSubscriber<std::unique_ptr<folly::IOBuf>, folly::exception_wrapper> &subscriber_ =
    makeMockSubscriber<std::unique_ptr<folly::IOBuf>, folly::exception_wrapper>();
  EXPECT_CALL(subscriber_, onSubscribe_(_));
  EXPECT_CALL(subscriber_, onComplete_());
  connection->setInput(subscriber_);
};

TEST(TcpDuplexConnectionTest, Send) {
  EventBase eventBase_;
  StrictMock<MockAsyncSocketWithWrite> *socket_ = new StrictMock<MockAsyncSocketWithWrite>(&eventBase_);
  EXPECT_CALL(*socket_, writeChain_(_, _, _));
  EXPECT_CALL(*socket_, closeNow()).Times(AtLeast(1));
  std::unique_ptr<TcpDuplexConnection> connection =
      folly::make_unique<TcpDuplexConnection>(AsyncSocket::UniquePtr(socket_));
  connection->send(folly::IOBuf::copyBuffer("derp"));
};
