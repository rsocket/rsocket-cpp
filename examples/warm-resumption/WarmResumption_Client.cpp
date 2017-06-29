// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "examples/util/ExampleSubscriber.h"
#include "src/RSocket.h"
#include "src/transports/tcp/TcpConnectionFactory.h"

#include "yarpl/Flowable.h"

using namespace rsocket_example;
using namespace rsocket;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

namespace {

class HelloSubscriber : public virtual yarpl::Refcounted,
                        public yarpl::flowable::Subscriber<Payload> {
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
  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
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

  void onError(std::exception_ptr) noexcept override {
    LOG(INFO) << "Received: onError ";
  }

 private:
  yarpl::Reference<yarpl::flowable::Subscription> subscription_{nullptr};
  std::atomic<int> count_{0};
};
}

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  folly::SocketAddress address;
  address.setFromHostPort(FLAGS_host, FLAGS_port);

  // create a client which can then make connections below
  auto client = RSocket::createClient(
      std::make_unique<TcpConnectionFactory>(std::move(address)));

  // connect and wait for connection
  SetupParameters setupParameters;
  setupParameters.resumable = true;
  auto rs = client->connect(std::move(setupParameters)).get();

  // subscriber
  auto subscriber = yarpl::make_ref<HelloSubscriber>();

  // perform request on connected RSocket
  rs->requestStream(Payload("Jane"))->subscribe(subscriber);

  subscriber->request(7);

  while (subscriber->rcvdCount() < 3) {
    std::this_thread::yield();
  }

  client->disconnect(std::runtime_error("disconnect triggered from client"));

  // This request should get buffered
  subscriber->request(3);

  client->resume(std::make_unique<DefaultResumeCallback>());

  subscriber->request(3);

  while (subscriber->rcvdCount() < 13) {
    std::this_thread::yield();
  }

  std::getchar();

  subscriber->cancel();

  std::getchar();

  return 0;
}
