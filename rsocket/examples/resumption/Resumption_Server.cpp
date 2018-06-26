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

#include <iostream>
#include <thread>

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/RSocketServiceHandler.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"

using namespace rsocket;
using namespace yarpl::flowable;

DEFINE_int32(port, 9898, "Port to accept connections on");

class HelloStreamRequestResponder : public RSocketResponder {
 public:
  std::shared_ptr<Flowable<rsocket::Payload>> handleRequestStream(
      rsocket::Payload request,
      rsocket::StreamId) override {
    auto requestString = request.moveDataToString();
    return Flowable<>::range(1, 1000)->map(
        [name = std::move(requestString)](int64_t v) {
          return Payload(folly::to<std::string>(v), "metadata");
        });
  }
};

class HelloServiceHandler : public RSocketServiceHandler {
 public:
  folly::Expected<RSocketConnectionParams, RSocketException> onNewSetup(
      const SetupParameters&) override {
    return RSocketConnectionParams(
        std::make_shared<HelloStreamRequestResponder>());
  }

  void onNewRSocketState(
      std::shared_ptr<RSocketServerState> state,
      ResumeIdentificationToken token) override {
    store_.lock()->insert({token, std::move(state)});
  }

  folly::Expected<std::shared_ptr<RSocketServerState>, RSocketException>
  onResume(ResumeIdentificationToken token) override {
    auto itr = store_->find(token);
    CHECK(itr != store_->end());
    return itr->second;
  };

 private:
  folly::Synchronized<
      std::map<ResumeIdentificationToken, std::shared_ptr<RSocketServerState>>,
      std::mutex>
      store_;
};

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  TcpConnectionAcceptor::Options opts;
  opts.address = folly::SocketAddress("::", FLAGS_port);
  opts.threads = 3;

  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  auto rawRs = rs.get();
  auto serverThread = std::thread(
      [=] { rawRs->startAndPark(std::make_shared<HelloServiceHandler>()); });

  std::getchar();

  rs->unpark();
  serverThread.join();

  return 0;
}
