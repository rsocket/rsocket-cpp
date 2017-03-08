// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketServer.h"
#include <folly/ExceptionWrapper.h>
#include <folly/io/async/EventBaseManager.h>
#include "rsocket/RSocketErrors.h"
#include "src/FrameTransport.h"

using namespace folly;
using namespace reactivesocket;

namespace rsocket {

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
      Executor& executor) {
    onSetup_(std::move(frameTransport), std::move(setupPayload), executor);
  }
  void resumeSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumeIdentificationToken,
      ResumePosition,
      Executor& executor) {
    //      onSetup_(std::move(frameTransport), std::move(setupPayload));
  }

 private:
  OnSetupNewSocket onSetup_;
};

RSocketServer::RSocketServer(
    std::unique_ptr<ConnectionAcceptor> connectionAcceptor)
    : lazyAcceptor_(std::move(connectionAcceptor)) {}

void RSocketServer::start(OnAccept onAccept) {
  if (!acceptor_) {
    LOG(INFO) << "RSocketServer => initialize connection acceptor on start";
    acceptor_ = std::make_unique<RSocketConnection>([
      this,
      onAccept = std::move(onAccept)
    ](std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload,
      Executor & executor_) {
      LOG(INFO) << "RSocketServer => received new setup payload";

      std::shared_ptr<RequestHandler> requestHandler;
      try {
        requestHandler = onAccept(
            std::make_unique<ConnectionSetupRequest>(std::move(setupPayload)));
      } catch (RSocketError& e) {
        // TODO emit ERROR ... but how do I do that here?
        frameTransport->close(
            folly::exception_wrapper{std::current_exception(), e});
        return;
      }
      LOG(INFO) << "RSocketServer => received request handler";

      auto rs = StandardReactiveSocket::disconnectedServer(
          // we know this callback is on a specific EventBase
          executor_,
          std::move(requestHandler),
          Stats::noop());

      rs->clientConnect(std::move(frameTransport));

      // register callback for removal when the socket closes
      rs->onClosed([ this, rs = rs.get() ](const folly::exception_wrapper& ex) {
        removeSocket(*rs);
        LOG(INFO) << "RSocketServer => removed closed connection";
      });

      // store this ReactiveSocket
      addSocket(std::move(rs));
    });
  } else {
    throw std::runtime_error("RSocketServer.start already called.");
  }
  lazyAcceptor_->start([ this, onAccept = std::move(onAccept) ](
      std::unique_ptr<DuplexConnection> duplexConnection, Executor & executor) {
    LOG(INFO) << "RSocketServer => received new connection";

    LOG(INFO) << "RSocketServer => going to accept duplex connection";
    // the callbacks above are wired up, now accept the connection
    acceptor_->acceptConnection(std::move(duplexConnection), executor);
  });
}

void RSocketServer::startAndPark(OnAccept onAccept) {
  start(std::forward<OnAccept>(onAccept));
  // now block this thread
  std::unique_lock<std::mutex> lk(m_);
  // if shutdown gets implemented this would then be released by it
  cv_.wait(lk, [] { return false; });
}

void RSocketServer::removeSocket(ReactiveSocket& socket) {
  LOG(INFO) << "RSocketServer => remove socket from vector";
  reactiveSockets_.erase(std::remove_if(
      reactiveSockets_.begin(),
      reactiveSockets_.end(),
      [&socket](std::unique_ptr<ReactiveSocket>& vecSocket) {
        return vecSocket.get() == &socket;
      }));
}
void RSocketServer::addSocket(std::unique_ptr<ReactiveSocket> socket) {
  LOG(INFO) << "RSocketServer => add socket to vector";
  reactiveSockets_.push_back(std::move(socket));
}
}
