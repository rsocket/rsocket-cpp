// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketServer.h"
#include <folly/ExceptionWrapper.h>
#include <folly/io/async/EventBaseManager.h>
#include "rsocket/RSocketErrors.h"
#include "src/FrameTransport.h"

using namespace reactivesocket;

namespace rsocket {

using OnSetupNewSocket = std::function<void(
    std::shared_ptr<FrameTransport> frameTransport,
    ConnectionSetupPayload setupPayload,
    folly::Executor&)>;

class RSocketConnection : public reactivesocket::ServerConnectionAcceptor {
 public:
  explicit RSocketConnection(OnSetupNewSocket onSetup)
      : onSetup_(std::move(onSetup)) {}

  ~RSocketConnection() {
    LOG(INFO) << "RSocketServer => destroy the connection acceptor";
  }

  void setupNewSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload,
      folly::Executor& executor) override {
    onSetup_(std::move(frameTransport), std::move(setupPayload), executor);
  }
  void resumeSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumeParameters,
      folly::Executor& executor) override {
    //      onSetup_(std::move(frameTransport), std::move(setupPayload));
  }

 private:
  OnSetupNewSocket onSetup_;
};

RSocketServer::RSocketServer(
    std::unique_ptr<ConnectionAcceptor> connectionAcceptor)
    : lazyAcceptor_(std::move(connectionAcceptor)) {}

RSocketServer::~RSocketServer() {
  {
    auto locked = sockets_.lock();
    if (locked->empty()) {
      return;
    }

    shutdown_.emplace();

    for (auto& socket : *locked) {
      // close() has to be called on the same executor as the socket.
      socket->executor().add([s = socket.get()] { s->close(); });
    }
  }

  shutdown_->wait();
  DCHECK(sockets_.lock()->empty());
}

void RSocketServer::start(OnAccept onAccept) {
  if (acceptor_) {
    throw std::runtime_error("RSocketServer::start() already called.");
  }

  LOG(INFO) << "RSocketServer => initialize connection acceptor on start";

  acceptor_ = std::make_unique<RSocketConnection>(
      [ this, onAccept = std::move(onAccept) ](
          std::shared_ptr<FrameTransport> frameTransport,
          ConnectionSetupPayload setupPayload,
          folly::Executor & executor) mutable {
        LOG(INFO) << "RSocketServer => received new setup payload";

        std::shared_ptr<RequestHandler> requestHandler;
        try {
          requestHandler = onAccept(std::make_unique<ConnectionSetupRequest>(
              std::move(setupPayload)));
        } catch (const RSocketError& e) {
          // TODO emit ERROR ... but how do I do that here?
          frameTransport->close(
              folly::exception_wrapper{std::current_exception(), e});
          return;
        }
        LOG(INFO) << "RSocketServer => received request handler";

        auto rs = StandardReactiveSocket::disconnectedServer(
            // we know this callback is on a specific EventBase
            executor,
            std::move(requestHandler),
            Stats::noop());

        rs->onClosed([ this, rs = rs.get() ](const folly::exception_wrapper&) {
          // Enqueue another event to remove and delete it.  We cannot delete
          // the ReactiveSocket now as it still needs to finish processing the
          // onClosed handlers in the stack frame above us.
          rs->executor().add([this, rs] { removeSocket(rs); });
        });

        auto rawRs = rs.get();

        addSocket(std::move(rs));

        // Connect last, after all state has been set up.
        rawRs->serverConnect(std::move(frameTransport), true /* resumable */);
      });

  lazyAcceptor_->start([this](
      std::unique_ptr<DuplexConnection> conn, folly::Executor& executor) {
    LOG(INFO) << "RSocketServer => received new connection";

    LOG(INFO) << "RSocketServer => going to accept duplex connection";
    // the callbacks above are wired up, now accept the connection
    acceptor_->acceptConnection(std::move(conn), executor);
  });
}

void RSocketServer::startAndPark(OnAccept onAccept) {
  start(std::move(onAccept));
  waiting_.wait();
}

void RSocketServer::unpark() {
  waiting_.post();
}

void RSocketServer::addSocket(std::unique_ptr<ReactiveSocket> socket) {
  sockets_.lock()->insert(std::move(socket));
}

void RSocketServer::removeSocket(ReactiveSocket* socket) {
  // This is a hack.  We make a unique_ptr so that we can use it to
  // search the set.  However, we release the unique_ptr so it doesn't
  // try to free the ReactiveSocket too.
  std::unique_ptr<ReactiveSocket> ptr{socket};

  auto locked = sockets_.lock();
  locked->erase(ptr);

  ptr.release();

  LOG(INFO) << "Removed ReactiveSocket";

  if (shutdown_ && locked->empty()) {
    shutdown_->post();
  }
}
}
