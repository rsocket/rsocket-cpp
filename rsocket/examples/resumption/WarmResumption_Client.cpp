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

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/examples/util/ExampleSubscriber.h"
#include "rsocket/internal/ClientResumeStatusCallback.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

#include "yarpl/Flowable.h"

using namespace rsocket;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

namespace {

class HelloSubscriber : public yarpl::flowable::Subscriber<Payload> {
 public:
  void request(int n) {
    LOG(INFO) << "... requesting " << n;
    while (!subscription_) {
      std::this_thread::yield();
    }
    subscription_->request(n);
  }

  void cancel() {
    if (auto subscription = std::move(subscription_)) {
      subscription->cancel();
    }
  }

  int rcvdCount() const {
    return count_;
  };

 protected:
  void onSubscribe(std::shared_ptr<yarpl::flowable::Subscription>
                       subscription) noexcept override {
    subscription_ = subscription;
  }

  void onNext(Payload element) noexcept override {
    LOG(INFO) << "Received: " << element.moveDataToString() << std::endl;
    count_++;
  }

  void onComplete() noexcept override {
    LOG(INFO) << "Received: onComplete";
  }

  void onError(folly::exception_wrapper) noexcept override {
    LOG(INFO) << "Received: onError ";
  }

 private:
  std::shared_ptr<yarpl::flowable::Subscription> subscription_{nullptr};
  std::atomic<int> count_{0};
};
} // namespace

std::unique_ptr<RSocketClient> getClientAndRequestStream(
    folly::EventBase* eventBase,
    std::shared_ptr<HelloSubscriber> subscriber) {
  folly::SocketAddress address;
  address.setFromHostPort(FLAGS_host, FLAGS_port);
  SetupParameters setupParameters;
  setupParameters.resumable = true;
  auto client = RSocket::createConnectedClient(
                    std::make_unique<TcpConnectionFactory>(
                        *eventBase, std::move(address)),
                    std::move(setupParameters))
                    .get();
  client->getRequester()->requestStream(Payload("Jane"))->subscribe(subscriber);
  return client;
}

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  folly::ScopedEventBaseThread worker1;

  auto subscriber1 = std::make_shared<HelloSubscriber>();
  auto client = getClientAndRequestStream(worker1.getEventBase(), subscriber1);

  subscriber1->request(7);

  while (subscriber1->rcvdCount() < 3) {
    std::this_thread::yield();
  }
  client->disconnect(std::runtime_error("disconnect triggered from client"));

  folly::ScopedEventBaseThread worker2;

  client->resume()
      .via(worker2.getEventBase())
      .thenValue([subscriber1](folly::Unit) {
        // continue with the old client.
        subscriber1->request(3);
        while (subscriber1->rcvdCount() < 10) {
          std::this_thread::yield();
        }
        subscriber1->cancel();
      })
      .thenError([&](folly::exception_wrapper ex) {
        LOG(INFO) << "Resumption Failed: " << ex.what();
        try {
          ex.throw_exception();
        } catch (const ResumptionException& e) {
          LOG(INFO) << "ResumptionException";
        } catch (const ConnectionException& e) {
          LOG(INFO) << "ConnectionException";
        } catch (const std::exception& e) {
          LOG(INFO) << "UnknownException " << typeid(e).name();
        }
        // Create a new client
        auto subscriber2 = std::make_shared<HelloSubscriber>();
        auto client =
            getClientAndRequestStream(worker1.getEventBase(), subscriber2);
        subscriber2->request(7);
        while (subscriber2->rcvdCount() < 7) {
          std::this_thread::yield();
        }
        subscriber2->cancel();
      });

  getchar();

  return 0;
}
