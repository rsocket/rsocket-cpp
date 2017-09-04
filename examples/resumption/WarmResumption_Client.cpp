// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "examples/util/ExampleSubscriber.h"
#include "rsocket/RSocket.h"
#include "rsocket/internal/ClientResumeStatusCallback.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

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

  void onError(folly::exception_wrapper) noexcept override {
    LOG(INFO) << "Received: onError ";
  }

 private:
  yarpl::Reference<yarpl::flowable::Subscription> subscription_{nullptr};
  std::atomic<int> count_{0};
};
}

std::unique_ptr<RSocketClient> getClientAndRequestStream(
    folly::EventBase* eventBase,
    yarpl::Reference<HelloSubscriber> subscriber) {
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

  auto subscriber1 = yarpl::make_ref<HelloSubscriber>();
  auto client = getClientAndRequestStream(worker1.getEventBase(), subscriber1);

  subscriber1->request(7);

  while (subscriber1->rcvdCount() < 3) {
    std::this_thread::yield();
  }
  client->disconnect(std::runtime_error("disconnect triggered from client"));

  folly::ScopedEventBaseThread worker2;

  client->resume()
      .via(worker2.getEventBase())
      .then([subscriber1] {
        // continue with the old client.
        subscriber1->request(3);
        while (subscriber1->rcvdCount() < 10) {
          std::this_thread::yield();
        }
        subscriber1->cancel();
      })
      .onError([&](folly::exception_wrapper ex) {
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
        auto subscriber2 = yarpl::make_ref<HelloSubscriber>();
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
