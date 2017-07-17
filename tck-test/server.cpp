// Copyright 2004-present Facebook. All Rights Reserved.

#include <fstream>
#include <future>
#include <signal.h>

#include <folly/Memory.h>
#include <folly/String.h>
#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/framing/FramedDuplexConnection.h"
#include "rsocket/transports/tcp/TcpDuplexConnection.h"
#include "rsocket/RSocket.h"

#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"

#include "tck-test/MarbleProcessor.h"

using namespace folly;
using namespace rsocket;
using namespace yarpl;
using namespace yarpl::flowable;
using namespace yarpl::single;

DEFINE_string(ip, "0.0.0.0", "IP to bind on");
DEFINE_int32(port, 9898, "port to listen to");
DEFINE_string(test_file, "../tck-test/servertest.txt", "test file to run");

namespace {

struct MarbleStore {
  std::map<std::pair<std::string, std::string>, std::string> reqRespMarbles;
  std::map<std::pair<std::string, std::string>, std::string> streamMarbles;
  std::map<std::pair<std::string, std::string>, std::string> channelMarbles;
};

MarbleStore parseMarbles(const std::string& fileName) {
  MarbleStore ms;

  std::ifstream input(fileName);
  if (!input.good()) {
    LOG(FATAL) << "Could not read from file '" << fileName << "'";
  }

  std::string line;
  while (std::getline(input, line)) {
    std::vector<folly::StringPiece> args;
    folly::split("%%", line, args);
    CHECK(args.size() == 4);
    if (args[0] == "rr") {
      ms.reqRespMarbles.emplace(
          std::make_pair(args[1].toString(), args[2].toString()),
          args[3].toString());
    } else if (args[0] == "rs") {
      ms.streamMarbles.emplace(
          std::make_pair(args[1].toString(), args[2].toString()),
          args[3].toString());
    } else if (args[0] == "channel") {
      ms.channelMarbles.emplace(
          std::make_pair(args[1].toString(), args[2].toString()),
          args[3].toString());
    } else {
      LOG(FATAL) << "Unrecognized token " << args[0];
    }
  }
  return ms;
}
}

class ServerResponder : public RSocketResponder {
 public:
  ServerResponder() {
    marbles_ = parseMarbles(FLAGS_test_file);
  }

  yarpl::Reference<Flowable<Payload>> handleRequestStream(
      Payload request,
      StreamId streamId) override {
    LOG(INFO) << "handleRequestStream " << request;
    std::string data = request.data->moveToFbString().toStdString();
    std::string metadata = request.metadata->moveToFbString().toStdString();
    auto it = marbles_.streamMarbles.find(std::make_pair(data, metadata));
    if (it == marbles_.streamMarbles.end()) {
      return yarpl::flowable::Flowables::error<rsocket::Payload>(
          std::logic_error("No MarbleHandler found"));
    } else {
      auto marbleProcessor = std::make_shared<tck::MarbleProcessor>(it->second);
      auto lambda = [marbleProcessor](
          yarpl::flowable::Subscriber<rsocket::Payload>& subscriber,
          int64_t requested) mutable {
        return marbleProcessor->run(subscriber, requested);
      };
      return Flowable<rsocket::Payload>::create(std::move(lambda));
    }
  }

  yarpl::Reference<Single<Payload>> handleRequestResponse(
      Payload request,
      StreamId streamId) override {
    LOG(INFO) << "handleRequestResponse " << request;
    std::string data = request.data->moveToFbString().toStdString();
    std::string metadata = request.metadata->moveToFbString().toStdString();
    auto it = marbles_.reqRespMarbles.find(std::make_pair(data, metadata));
    if (it == marbles_.reqRespMarbles.end()) {
      return yarpl::single::Singles::error<rsocket::Payload>(
          std::logic_error("No MarbleHandler found"));
    } else {
      auto marbleProcessor = std::make_shared<tck::MarbleProcessor>(it->second);
      auto lambda = [marbleProcessor](
          yarpl::Reference<yarpl::single::SingleObserver<rsocket::Payload>>
              subscriber) {
        subscriber->onSubscribe(SingleSubscriptions::empty());
        return marbleProcessor->run(subscriber);
      };
      return Single<rsocket::Payload>::create(std::move(lambda));
    }
  }

 private:
  MarbleStore marbles_;
};

std::promise<void> terminate;

static void signal_handler(int signal) {
  LOG(INFO) << "Terminating program after receiving signal " << signal;
  terminate.set_value();
}

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  folly::init(&argc, &argv);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  TcpConnectionAcceptor::Options opts;
  opts.port = FLAGS_port;
  opts.threads = 2;

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  // global request handler
  auto responder = std::make_shared<ServerResponder>();

  auto rawRs = rs.get();
  auto serverThread = std::thread([=] {
    // start accepting connections
    rawRs->startAndPark([responder](auto& setup) { setup.createRSocket(responder); });
  });

  terminate.get_future().wait();
  rs->unpark();
  serverThread.join();

  return 0;
}
