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

#include <atomic>
#include <iostream>
#include <sstream>
#include <thread>

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "yarpl/Observable.h"

using namespace rsocket;
using namespace yarpl;
using namespace yarpl::flowable;
using namespace yarpl::observable;

DEFINE_int32(port, 9898, "port to connect to");

class PushStreamRequestResponder : public rsocket::RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  std::shared_ptr<Flowable<Payload>> handleRequestStream(
      Payload request,
      rsocket::StreamId) override {
    std::cout << "PushStreamRequestResponder.handleRequestStream " << request
              << std::endl;

    // string from payload data
    auto requestString = request.moveDataToString();

    // Simulate a pure push infinite event stream
    // The Observable type is well suited to this
    // as it is easy to emit events to without
    // concern for flow control, which makes sense
    // for event sources that don't support flow control.
    //
    // Since this is being returned to the network layer
    // it must be converted into a Flowable which supports
    // flow control. This is done using Observable->toFlowable
    // which accepts a BackpressureStrategy.
    //
    // This examples uses BackpressureStrategy::DROP which simply
    // drops any events emitted from the Observable if the Flowable
    // does not have any credits from the Subscriber.
    return Observable<Payload>::create(
               [name = std::move(requestString)](
                   std::shared_ptr<Observer<Payload>> s) {
                 // Must make this async since it's an infinite stream
                 // and will block the IO thread.
                 // Using a raw thread right now since the 'subscribeOn'
                 // operator is not ready yet. This can eventually
                 // be replaced with use of 'subscribeOn'.
                 std::thread([s, name]() {
                   int64_t v = 0;
                   while (!s->isUnsubscribed()) {
                     std::stringstream ss;
                     ss << "Event[" << name << "]-" << ++v << "!";
                     std::string payloadData = ss.str();
                     s->onNext(Payload(payloadData, "metadata"));
                   }
                 }).detach();
               })
        ->toFlowable(BackpressureStrategy::DROP);
  }
};

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  TcpConnectionAcceptor::Options opts;
  opts.address = folly::SocketAddress("::", FLAGS_port);
  opts.threads = 2;

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  auto rawRs = rs.get();
  auto serverThread = std::thread([=] {
    // start accepting connections
    rawRs->startAndPark([](const SetupParameters&) {
      return std::make_shared<PushStreamRequestResponder>();
    });
  });

  // Wait for a newline on the console to terminate the server.
  std::getchar();

  rs->unpark();
  serverThread.join();

  return 0;
}
