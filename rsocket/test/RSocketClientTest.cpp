// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "RSocketTests.h"

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>

#include "rsocket/test/test_utils/MockDuplexConnection.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

using namespace rsocket;
using namespace testing;
using namespace yarpl::single;

TEST(RSocketClient, ConnectFails) {
  folly::ScopedEventBaseThread worker;

  folly::SocketAddress address;
  address.setFromHostPort("localhost", 1);
  auto client =
      RSocket::createConnectedClient(std::make_unique<TcpConnectionFactory>(
          *worker.getEventBase(), std::move(address)));

  std::move(client)
      .thenValue([&](auto&&) { FAIL() << "the test needs to fail"; })
      .thenError(
          folly::tag_t<std::exception>{},
          [&](const std::exception&) {
            LOG(INFO) << "connection failed as expected";
          })
      .get();
}

TEST(RSocketClient, PreallocatedBytesInFrames) {
  auto connection = std::make_unique<MockDuplexConnection>();
  EXPECT_CALL(*connection, isFramed()).WillRepeatedly(Return(true));

  // SETUP frame and FIRE_N_FORGET frame send
  EXPECT_CALL(*connection, send_(_))
      .Times(2)
      .WillRepeatedly(
          Invoke([](std::unique_ptr<folly::IOBuf>& serializedFrame) {
            // we should have headroom preallocated for the frame size field
            EXPECT_EQ(
                FrameSerializer::createFrameSerializer(ProtocolVersion::Latest)
                    ->frameLengthFieldSize(),
                serializedFrame->headroom());
          }));

  folly::ScopedEventBaseThread worker;

  worker.getEventBase()->runInEventBaseThread([&] {
    auto client = RSocket::createClientFromConnection(
        std::move(connection), *worker.getEventBase());

    client->getRequester()
        ->fireAndForget(Payload("hello"))
        ->subscribe(SingleObservers::create<void>());
  });
}
