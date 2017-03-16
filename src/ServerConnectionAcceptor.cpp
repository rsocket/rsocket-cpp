// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ServerConnectionAcceptor.h"
#include <folly/ExceptionWrapper.h>
#include "src/DuplexConnection.h"
#include "src/Frame.h"
#include "src/FrameProcessor.h"
#include "src/FrameTransport.h"
#include "src/Stats.h"

namespace reactivesocket {

ServerConnectionAcceptor::~ServerConnectionAcceptor() {
  for (auto& connection : connections_) {
    connection->close(std::runtime_error("ServerConnectionAcceptor closed"));
  }
}

void ServerConnectionAcceptor::processFrame(
    std::shared_ptr<FrameTransport> transport,
    std::unique_ptr<folly::IOBuf> frame,
    folly::Executor& executor) {
  removeConnection(transport);

  switch (frameSerializer_.peekFrameType(*frame)) {
    case FrameType::SETUP: {
      Frame_SETUP setupFrame;
      if (!frameSerializer_.deserializeFrom(setupFrame, std::move(frame))) {
        transport->outputFrameOrEnqueue(
            frameSerializer_.serializeOut(Frame_ERROR::invalidFrame()));
        transport->close(folly::exception_wrapper());
        break;
      }

      ConnectionSetupPayload setupPayload;
      setupFrame.moveToSetupPayload(setupPayload);

      transport->setFrameProcessor(nullptr);
      setupNewSocket(std::move(transport), std::move(setupPayload), executor);
      break;
    }

    case FrameType::RESUME: {
      Frame_RESUME resumeFrame;
      if (!frameSerializer_.deserializeFrom(resumeFrame, std::move(frame))) {
        transport->outputFrameOrEnqueue(
            frameSerializer_.serializeOut(Frame_ERROR::invalidFrame()));
        transport->close(folly::exception_wrapper());
        break;
      }

      transport->setFrameProcessor(nullptr);
      resumeSocket(
          std::move(transport),
          std::move(resumeFrame.token_),
          resumeFrame.lastReceivedServerPosition_,
          resumeFrame.clientPosition_,
          executor);
    } break;

    case FrameType::CANCEL:
    case FrameType::ERROR:
    case FrameType::KEEPALIVE:
    case FrameType::LEASE:
    case FrameType::METADATA_PUSH:
    case FrameType::REQUEST_CHANNEL:
    case FrameType::REQUEST_FNF:
    case FrameType::REQUEST_N:
    case FrameType::REQUEST_RESPONSE:
    case FrameType::REQUEST_STREAM:
    case FrameType::RESERVED:
    case FrameType::PAYLOAD:
    case FrameType::RESUME_OK:
    case FrameType::EXT:
    default: {
      transport->outputFrameOrEnqueue(
          frameSerializer_.serializeOut(Frame_ERROR::unexpectedFrame()));
      transport->close(folly::exception_wrapper());
      break;
    }
  }
}

void ServerConnectionAcceptor::removeConnection(
    const std::shared_ptr<FrameTransport>& transport) {
  connections_.erase(transport);
}

class OneFrameProcessor
    : public FrameProcessor,
      public std::enable_shared_from_this<OneFrameProcessor> {
 public:
  OneFrameProcessor(
      ServerConnectionAcceptor& acceptor,
      std::shared_ptr<FrameTransport> transport,
      folly::Executor& executor)
      : acceptor_(acceptor),
        transport_(std::move(transport)),
        executor_(executor) {
    DCHECK(transport_);
  }

  void processFrame(std::unique_ptr<folly::IOBuf> buf) override {
    acceptor_.processFrame(transport_, std::move(buf), executor_);
    // no more code here as the instance might be gone by now
  }

  void onTerminal(folly::exception_wrapper ex) override {
    acceptor_.removeConnection(transport_);
    transport_->close(std::move(ex));
    // no more code here as the instance might be gone by now
  }

 private:
  ServerConnectionAcceptor& acceptor_;
  std::shared_ptr<FrameTransport> transport_;
  folly::Executor& executor_;
};

void ServerConnectionAcceptor::acceptConnection(
    std::unique_ptr<DuplexConnection> connection,
    folly::Executor& executor) {
  auto transport = std::make_shared<FrameTransport>(std::move(connection));
  auto processor =
      std::make_shared<OneFrameProcessor>(*this, transport, executor);
  connections_.insert(transport);
  // transport can receive frames right away
  transport->setFrameProcessor(std::move(processor));
}

} // reactivesocket
