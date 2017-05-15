// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketConnectionHandler.h"

#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventBaseManager.h>

#include "rsocket/OldNewBridge.h"
#include "rsocket/RSocketErrors.h"
#include "src/NullRequestHandler.h"

namespace rsocket {

using namespace reactivesocket;

class RSocketHandlerBridge : public reactivesocket::DefaultRequestHandler {
 public:
  RSocketHandlerBridge(std::shared_ptr<RSocketRequestHandler> handler)
      : handler_(std::move(handler)){};

  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>&
          response) noexcept override {
    auto flowable =
        handler_->handleRequestStream(std::move(request), std::move(streamId));
    // bridge from the existing eager RequestHandler and old Subscriber type
    // to the lazy Flowable and new Subscriber type
    flowable->subscribe(yarpl::Reference<yarpl::flowable::Subscriber<Payload>>(
        new NewToOldSubscriber(std::move(response))));
  }

  std::shared_ptr<Subscriber<Payload>> handleRequestChannel(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) noexcept override {
    auto eagerSubscriber = std::make_shared<EagerSubscriberBridge>();
    auto flowable = handler_->handleRequestChannel(
        std::move(request),
        yarpl::flowable::Flowables::fromPublisher<Payload>(
        [eagerSubscriber](yarpl::Reference<yarpl::flowable::Subscriber<Payload>> subscriber) {
            eagerSubscriber->subscribe(subscriber);
        }),
        std::move(streamId));
    // bridge from the existing eager RequestHandler and old Subscriber type
    // to the lazy Flowable and new Subscriber type
    flowable->subscribe(yarpl::Reference<yarpl::flowable::Subscriber<Payload>>(
        new NewToOldSubscriber(std::move(response))));

    return eagerSubscriber;
  }

 private:
  std::shared_ptr<RSocketRequestHandler> handler_;
};

void RSocketConnectionHandler::setupNewSocket(
    std::shared_ptr<FrameTransport> frameTransport,
    ConnectionSetupPayload setupPayload) {
  LOG(INFO) << "RSocketServer => received new setup payload";

  // FIXME(alexanderm): Handler should be tied to specific executor
  auto executor = folly::EventBaseManager::get()->getExistingEventBase();

  auto socketParams =
      SocketParameters(setupPayload.resumable, setupPayload.protocolVersion);
  std::shared_ptr<ConnectionSetupRequest> setupRequest =
      std::make_shared<ConnectionSetupRequest>(std::move(setupPayload));
  std::shared_ptr<RSocketRequestHandler> requestHandler;
  try {
    requestHandler = getHandler(setupRequest);
  } catch (const RSocketError& e) {
    // TODO emit ERROR ... but how do I do that here?
    frameTransport->close(
        folly::exception_wrapper{std::current_exception(), e});
    return;
  }
  LOG(INFO) << "RSocketServer => received request handler";

  auto handlerBridge =
      std::make_shared<RSocketHandlerBridge>(std::move(requestHandler));
  auto rs = ReactiveSocket::disconnectedServer(
      // we know this callback is on a specific EventBase
      *executor,
      std::move(handlerBridge),
      Stats::noop());

  auto rawRs = rs.get();

  manageSocket(setupRequest, std::move(rs));

  // Connect last, after all state has been set up.
  rawRs->serverConnect(std::move(frameTransport), socketParams);
}

bool RSocketConnectionHandler::resumeSocket(
    std::shared_ptr<FrameTransport> frameTransport,
    ResumeParameters) {
  return false;
}

void RSocketConnectionHandler::connectionError(
    std::shared_ptr<FrameTransport>,
    folly::exception_wrapper ex) {
  LOG(WARNING) << "Connection failed before first frame: " << ex.what();
}

} // namespace rsocket
