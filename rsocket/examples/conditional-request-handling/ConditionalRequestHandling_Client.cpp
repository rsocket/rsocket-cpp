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

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>
#include <iostream>

#include "rsocket/RSocket.h"
#include "rsocket/examples/util/ExampleSubscriber.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"
#include "yarpl/Flowable.h"

using namespace ::folly;
using namespace ::rsocket;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

namespace {
class ChannelConnectionEvents : public RSocketConnectionEvents {
 public:
  void onConnected() override {
    LOG(INFO) << "onConnected";
  }

  void onDisconnected(const folly::exception_wrapper& ex) override {
    LOG(INFO) << "onDiconnected ex=" << ex.what();
  }

  void onClosed(const folly::exception_wrapper& ex) override {
    LOG(INFO) << "onClosed ex=" << ex.what();
    closed_ = true;
  }

  bool isClosed() const {
    return closed_;
  }

 private:
  std::atomic<bool> closed_{false};
};
} // namespace

void sendRequest(std::string mimeType) {
  folly::ScopedEventBaseThread worker;
  folly::SocketAddress address;
  address.setFromHostPort(FLAGS_host, FLAGS_port);
  auto connectionEvents = std::make_shared<ChannelConnectionEvents>();
  auto client = RSocket::createConnectedClient(
                    std::make_unique<TcpConnectionFactory>(
                        *worker.getEventBase(), std::move(address)),
                    SetupParameters(mimeType, mimeType),
                    std::make_shared<RSocketResponder>(),
                    kDefaultKeepaliveInterval,
                    RSocketStats::noop(),
                    connectionEvents)
                    .get();

  std::atomic<int> rcvdCount{0};

  client->getRequester()
      ->requestStream(Payload("Bob"))
      ->take(5)
      ->subscribe([&rcvdCount](Payload p) {
        std::cout << "Received: " << p.moveDataToString() << std::endl;
        rcvdCount++;
      });

  while (rcvdCount < 5 && !connectionEvents->isClosed()) {
    std::this_thread::yield();
  }
}

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  sendRequest("application/json");
  sendRequest("text/plain");
  sendRequest("garbage");

  return 0;
}
