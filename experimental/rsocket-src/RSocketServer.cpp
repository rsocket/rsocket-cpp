
#include "rsocket/RSocketServer.h"
#include <folly/io/async/EventBaseManager.h>
// TODO deal with name collision of src/ConnectionAcceptor
// and rsocket/ConnectionAcceptor
#include "src/ServerConnectionAcceptor.h"

namespace rsocket {

class RSocketConnection : public reactivesocket::ServerConnectionAcceptor {
 public:
  RSocketConnection(OnSetupNewSocket onSetup) : onSetup_(std::move(onSetup)) {}
  ~RSocketConnection() {
    LOG(INFO) << "RSocketServer => destroy the connection acceptor";
  }

  void setupNewSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload,
      EventBase& eventBase) {
    onSetup_(std::move(frameTransport), std::move(setupPayload), eventBase);
  }
  void resumeSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumeIdentificationToken,
      ResumePosition,
      EventBase& eventBase) {
    //      onSetup_(std::move(frameTransport), std::move(setupPayload));
  }

 private:
  OnSetupNewSocket onSetup_;
};

RSocketServer::RSocketServer(
    std::unique_ptr<ConnectionAcceptor> connectionAcceptor)
    : lazyAcceptor(std::move(connectionAcceptor)) {}

void RSocketServer::start(std::function<std::shared_ptr<RequestHandler>(
                              std::unique_ptr<ConnectionRequest>)> onAccept) {
  if (!acceptor_) {
    LOG(INFO) << "RSocketServer => initialize connection acceptor on start";
    acceptor_ = std::make_unique<RSocketConnection>([
      this,
      onAccept = std::move(onAccept)
    ](std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload,
      EventBase & eventBase_) {
      LOG(INFO) << "RSocketServer => received new setup payload";

      auto requestHandler = onAccept(
          std::make_unique<ConnectionRequest>(std::move(setupPayload)));

      LOG(INFO) << "RSocketServer => received request handler";

      auto rs = StandardReactiveSocket::disconnectedServer(
          // we know this callback is on a specific EventBase
          eventBase_,
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
  lazyAcceptor->start([ this, onAccept = std::move(onAccept) ](
      std::unique_ptr<DuplexConnection> duplexConnection,
      EventBase & eventBase) {
    LOG(INFO) << "RSocketServer => received new connection";

    LOG(INFO) << "RSocketServer => going to accept duplex connection";
    // the callbacks above are wired up, now accept the connection
    acceptor_->acceptConnection(std::move(duplexConnection), eventBase);
  });
}

void RSocketServer::shutdown() {
  shuttingDown = true;
  reactiveSockets_.clear();
}

void RSocketServer::removeSocket(ReactiveSocket& socket) {
  if (!shuttingDown) {
    LOG(INFO) << "RSocketServer => remove socket from vector";
    reactiveSockets_.erase(std::remove_if(
        reactiveSockets_.begin(),
        reactiveSockets_.end(),
        [&socket](std::unique_ptr<ReactiveSocket>& vecSocket) {
          return vecSocket.get() == &socket;
        }));
  }
}
void RSocketServer::addSocket(std::unique_ptr<ReactiveSocket> socket) {
  LOG(INFO) << "RSocketServer => add socket to vector";
  reactiveSockets_.push_back(std::move(socket));
}
}