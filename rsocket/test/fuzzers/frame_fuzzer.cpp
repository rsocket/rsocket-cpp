// Copyright 2004-present Facebook. All Rights Reserved.
#include <folly/String.h>
#include <folly/io/async/EventBaseManager.h>
#include <iostream>

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>
#include <glog/logging.h>

#include "rsocket/ConnectionAcceptor.h"
#include "rsocket/DuplexConnection.h"
#include "rsocket/RSocketServer.h"

struct FuzzerConnectionAcceptor : rsocket::ConnectionAcceptor {
  void start(rsocket::OnDuplexConnectionAccept func_) override {
    VLOG(1) << "FuzzerConnectionAcceptor::start()" << std::endl;
    func = func_;
  }

  void stop() override {
    VLOG(1) << "FuzzerConnectionAcceptor::stop()" << std::endl;
  }

  folly::Optional<uint16_t> listeningPort() const override {
    return 0;
  }

  rsocket::OnDuplexConnectionAccept func;
};

struct FuzzerDuplexConnection : rsocket::DuplexConnection {
  using Subscriber = rsocket::DuplexConnection::Subscriber;
  using DuplexSubscriber = rsocket::DuplexConnection::DuplexSubscriber;

  FuzzerDuplexConnection() {}

  void setInput(yarpl::Reference<Subscriber> sub) override {
    VLOG(1) << "FuzzerDuplexConnection::setInput()" << std::endl;
    input_sub = sub;
  }

  void send(std::unique_ptr<folly::IOBuf> buf) override {
    VLOG(1) << "FuzzerDuplexConnection::send(\""
            << folly::humanify(buf->moveToFbString()) << "\")" << std::endl;
  }

  yarpl::Reference<Subscriber> input_sub;
};

struct NoopSubscription : yarpl::flowable::Subscription {
  void request(int64_t n) override {
    VLOG(1) << "NoopSubscription::request(" << n << ")";
  }
  void cancel() override {
    VLOG(1) << "NoopSubscription::cancel()";
  }
};

struct NoopResponder : rsocket::RSocketResponder {};

std::string get_stdin() {
  std::cin >> std::noskipws;
  std::istream_iterator<char> it(std::cin);
  std::istream_iterator<char> end;
  std::string input(it, end);
  return input;
}

int main(int argc, char* argv[]) {
  folly::init(&argc, &argv);
  FLAGS_logtostderr = 1;

  folly::EventBase evb;
  folly::EventBaseManager::get()->setEventBase(&evb, false);

  auto feed_conn = std::make_unique<FuzzerDuplexConnection>();
  auto acceptor = std::make_unique<FuzzerConnectionAcceptor>();

  // grab references while we still own the duplex connection
  auto& input_sub = feed_conn->input_sub;
  auto& acceptor_func_ptr = acceptor->func;

  rsocket::RSocketServer server(std::move(acceptor));

  auto responder = std::make_shared<NoopResponder>();
  server.start(
      [responder](const rsocket::SetupParameters&) { return responder; });

  CHECK(acceptor_func_ptr);
  acceptor_func_ptr(std::move(feed_conn), evb);
  evb.loopOnce();

  CHECK(input_sub);
  auto input_subscription = yarpl::make_ref<NoopSubscription>();
  input_sub->onSubscribe(input_subscription);

  std::string fuzz_input = get_stdin();
  std::unique_ptr<folly::IOBuf> buf =
      folly::IOBuf::wrapBuffer(fuzz_input.c_str(), fuzz_input.size());

  VLOG(1) << "fuzz input: " << std::endl;
  VLOG(1) << folly::humanify(buf->cloneAsValue().moveToFbString()) << std::endl;

  input_sub->onNext(std::move(buf));
  evb.loopOnce();

  return 0;
}
