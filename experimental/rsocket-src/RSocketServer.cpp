// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketServer.h"
#include <folly/ExceptionWrapper.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventBaseManager.h>
#include "rsocket/OldNewBridge.h"
#include "rsocket/RSocketErrors.h"
#include "src/FrameTransport.h"
#include "src/NullRequestHandler.h"

using namespace reactivesocket;

namespace rsocket {

using OnSetupNewSocket = std::function<void(
    std::shared_ptr<FrameTransport> frameTransport,
    ConnectionSetupPayload setupPayload,
    folly::Executor&)>;

class RSocketConnectionHandler : public reactivesocket::ConnectionHandler {
 public:
  explicit RSocketConnectionHandler(OnSetupNewSocket onSetup)
      : onSetup_(std::move(onSetup)) {}

  ~RSocketConnectionHandler() {
    LOG(INFO) << "RSocketServer => destroy the connection handler";
  }

  void setupNewSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload) override {
    // FIXME(alexanderm): Handler should be tied to specific executor
    auto executor = folly::EventBaseManager::get()->getExistingEventBase();
    onSetup_(std::move(frameTransport), std::move(setupPayload), *executor);
  }

  bool resumeSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumeParameters) override {
    //      onSetup_(std::move(frameTransport), std::move(setupPayload));
    return false;
  }

  void connectionError(
      std::shared_ptr<FrameTransport>,
      folly::exception_wrapper ex) override {
    LOG(WARNING) << "Connection failed before first frame: " << ex.what();
  }

 private:
  OnSetupNewSocket onSetup_;
};

class RSocketHandlerBridge : public reactivesocket::DefaultRequestHandler {
 public:
  RSocketHandlerBridge(std::shared_ptr<RSocketRequestHandler> handler)
      : handler_(std::move(handler)){};

  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>&
          subscriber) noexcept override {
    auto flowable =
        handler_->handleRequestStream(std::move(request), std::move(streamId));
    // bridge from the existing eager RequestHandler and old Subscriber type
    // to the lazy Flowable and new Subscriber type
    flowable->subscribe(yarpl::Reference<yarpl::Subscriber<Payload>>(
        new NewToOldSubscriber(std::move(subscriber))));
  }

 private:
  std::shared_ptr<RSocketRequestHandler> handler_;
};

RSocketServer::RSocketServer(
    std::unique_ptr<ConnectionAcceptor> connectionAcceptor)
    : lazyAcceptor_(std::move(connectionAcceptor)),
      acceptor_(ProtocolVersion::Unknown) {}

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
  if (connectionHandler_) {
    throw std::runtime_error("RSocketServer::start() already called.");
  }

  LOG(INFO) << "RSocketServer => initialize connection acceptor on start";

  LOG(INFO) << "RSocketServer => initialize connection acceptor on start";
  connectionHandler_ = std::make_unique<RSocketConnectionHandler>(
      [ this, onAccept = std::move(onAccept) ](
          std::shared_ptr<FrameTransport> frameTransport,
          ConnectionSetupPayload setupPayload,
          folly::Executor & executor_) {
        LOG(INFO) << "RSocketServer => received new setup payload";

        auto socketParams = SocketParameters(
            setupPayload.resumable, setupPayload.protocolVersion);
        std::shared_ptr<RSocketRequestHandler> requestHandler;
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

        auto handlerBridge = std::make_shared<RSocketHandlerBridge>(std::move(requestHandler));
        auto rs = ReactiveSocket::disconnectedServer(
            // we know this callback is on a specific EventBase
            executor_,
            std::move(handlerBridge),
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
        rawRs->serverConnect(std::move(frameTransport), socketParams);
      });

  lazyAcceptor_
      ->start([this](
          std::unique_ptr<DuplexConnection> conn, folly::Executor& executor) {
        LOG(INFO) << "RSocketServer => received new connection";

        LOG(INFO) << "RSocketServer => going to accept duplex connection";
        // the callbacks above are wired up, now accept the connection
        // FIXME(alexanderm): This isn't thread safe
        acceptor_.accept(std::move(conn), connectionHandler_);
      })
      .onError([](const folly::exception_wrapper& ex) {
        LOG(FATAL) << "RSocketServer => failed to start HttpAcceptor: "
                   << ex.what();
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
