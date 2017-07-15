// Copyright 2004-present Facebook. All Rights Reserved.

#include <atomic>
#include <iostream>
#include <thread>

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "yarpl/Observable.h"
#include "yarpl/schedulers/ThreadScheduler.h"

using namespace rsocket;
using namespace yarpl;
using namespace yarpl::flowable;
using namespace yarpl::observable;

DEFINE_int32(port, 9898, "port to connect to");

class PushStreamRequestResponder : public rsocket::RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  yarpl::Reference<Flowable<Payload>> handleRequestStream(
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
    return Observable<Payload>::create([name = std::move(requestString)](
                                           Reference<Observer<Payload>> s) {
             // Must make this async since it's an infinite stream
             // and will block the IO thread.
             // Using a raw thread right now since the 'subscribeOn'
             // operator is not ready yet. This can eventually
             // be replaced with use of 'subscribeOn'.
             std::thread([s, name]() {
               auto subscription = Subscriptions::atomicBoolSubscription();
               s->onSubscribe(subscription);
               int64_t v = 0;
               while (!subscription->isCancelled()) {
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
  opts.port = FLAGS_port;
  opts.threads = 2;

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  // global request responder
  auto responder = std::make_shared<PushStreamRequestResponder>();

  auto rawRs = rs.get();
  auto serverThread = std::thread([=] {
    // start accepting connections
    rawRs->startAndPark([responder](auto& setup) { setup.createRSocket(responder); });
  });

  // Wait for a newline on the console to terminate the server.
  std::getchar();

  rs->unpark();
  serverThread.join();

  return 0;
}
