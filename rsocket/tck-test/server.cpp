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

#include <signal.h>
#include <fstream>
#include <future>

#include <folly/Memory.h>
#include <folly/String.h>
#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/RSocketServiceHandler.h"
#include "rsocket/framing/FramedDuplexConnection.h"
#include "rsocket/transports/tcp/TcpDuplexConnection.h"

#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"

#include "rsocket/tck-test/MarbleProcessor.h"

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
} // namespace

class ServerResponder : public RSocketResponder {
 public:
  ServerResponder() {
    marbles_ = parseMarbles(FLAGS_test_file);
  }

  std::shared_ptr<Flowable<Payload>> handleRequestStream(
      Payload request,
      StreamId) override {
    LOG(INFO) << "handleRequestStream " << request;
    const std::string data = request.data->moveToFbString().toStdString();
    const std::string metadata =
        request.metadata->moveToFbString().toStdString();
    const auto it = marbles_.streamMarbles.find(std::make_pair(data, metadata));
    if (it == marbles_.streamMarbles.end()) {
      return yarpl::flowable::Flowable<rsocket::Payload>::error(
          std::logic_error("No MarbleHandler found"));
    } else {
      const auto marbleProcessor =
          std::make_shared<tck::MarbleProcessor>(it->second);
      auto lambda = [marbleProcessor](
                        auto& subscriber, int64_t requested) mutable {
        return marbleProcessor->run(subscriber, requested);
      };
      return Flowable<rsocket::Payload>::create(std::move(lambda));
    }
  }

  std::shared_ptr<Single<Payload>> handleRequestResponse(
      Payload request,
      StreamId) override {
    LOG(INFO) << "handleRequestResponse " << request;
    const std::string data = request.data->moveToFbString().toStdString();
    const std::string metadata =
        request.metadata->moveToFbString().toStdString();
    const auto it =
        marbles_.reqRespMarbles.find(std::make_pair(data, metadata));
    if (it == marbles_.reqRespMarbles.end()) {
      return yarpl::single::Singles::error<rsocket::Payload>(
          std::logic_error("No MarbleHandler found"));
    } else {
      const auto marbleProcessor =
          std::make_shared<tck::MarbleProcessor>(it->second);
      auto lambda =
          [marbleProcessor](
              std::shared_ptr<yarpl::single::SingleObserver<rsocket::Payload>>
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

class ServiceHandler : public RSocketServiceHandler {
 public:
  folly::Expected<RSocketConnectionParams, RSocketException> onNewSetup(
      const SetupParameters&) override {
    return RSocketConnectionParams(std::make_shared<ServerResponder>());
  }

  void onNewRSocketState(
      std::shared_ptr<RSocketServerState> state,
      ResumeIdentificationToken token) override {
    store_.lock()->insert({token, std::move(state)});
  }

  folly::Expected<std::shared_ptr<RSocketServerState>, RSocketException>
  onResume(ResumeIdentificationToken token) override {
    const auto itr = store_->find(token);
    CHECK(itr != store_->end());
    return itr->second;
  };

 private:
  folly::Synchronized<
      std::map<ResumeIdentificationToken, std::shared_ptr<RSocketServerState>>,
      std::mutex>
      store_;
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
  opts.address = folly::SocketAddress(FLAGS_ip, FLAGS_port);
  opts.threads = 1;

  // RSocket server accepting on TCP
  const auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  const auto rawRs = rs.get();
  auto serverThread = std::thread(
      [=] { rawRs->startAndPark(std::make_shared<ServiceHandler>()); });

  terminate.get_future().wait();
  rs->unpark();
  serverThread.join();

  return 0;
}
