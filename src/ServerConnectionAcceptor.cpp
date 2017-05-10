// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ServerConnectionAcceptor.h"
#include <folly/ExceptionWrapper.h>
#include "src/DuplexConnection.h"
#include "src/Frame.h"
#include "src/FrameProcessor.h"
#include "src/FrameSerializer.h"
#include "src/FrameTransport.h"
#include "src/Stats.h"

namespace reactivesocket {

class OneFrameProcessor : public FrameProcessor {
 public:
  OneFrameProcessor(
      ServerConnectionAcceptor& acceptor,
      std::shared_ptr<FrameTransport> transport,
      std::shared_ptr<ConnectionHandler> connectionHandler)
      : acceptor_(acceptor),
        transport_(std::move(transport)),
        connectionHandler_(std::move(connectionHandler)) {
    DCHECK(transport_);
  }

  void processFrame(std::unique_ptr<folly::IOBuf> buf) override {
    acceptor_.processFrame(connectionHandler_, transport_, std::move(buf));
    // No more code here as the instance might be gone by now.
  }

  void onTerminal(folly::exception_wrapper ex) override {
    acceptor_.closeAndRemoveConnection(
        connectionHandler_, transport_, std::move(ex));
    // No more code here as the instance might be gone by now.
  }

 private:
  ServerConnectionAcceptor& acceptor_;
  std::shared_ptr<FrameTransport> transport_;
  std::shared_ptr<ConnectionHandler> connectionHandler_;
};

ServerConnectionAcceptor::ServerConnectionAcceptor(
    ProtocolVersion protocolVersion) {
  // If protocolVersion is unknown we will try to autodetect the version with
  // the first frame.
  if (protocolVersion != ProtocolVersion::Unknown) {
    defaultFrameSerializer_ =
        FrameSerializer::createFrameSerializer(protocolVersion);
  }
}

ServerConnectionAcceptor::~ServerConnectionAcceptor() {
  stop();
}

void ServerConnectionAcceptor::processFrame(
    std::shared_ptr<ConnectionHandler> connectionHandler,
    std::shared_ptr<FrameTransport> transport,
    std::unique_ptr<folly::IOBuf> frame) {

  // Helper for sending connection errors.
  auto error = [this, connectionHandler, transport](folly::StringPiece msg) {
    closeAndRemoveConnection(
        connectionHandler, transport, std::runtime_error(msg.str()));
  };

  auto frameSerializer = getOrAutodetectFrameSerializer(*frame);
  if (!frameSerializer) {
    error("Unable to detect protocol version");
    return;
  }

  switch (frameSerializer->peekFrameType(*frame)) {
    case FrameType::SETUP: {
      Frame_SETUP setupFrame;
      if (!frameSerializer->deserializeFrom(setupFrame, std::move(frame))) {
        transport->outputFrameOrEnqueue(
            frameSerializer->serializeOut(Frame_ERROR::invalidFrame()));
        error("Invalid SETUP frame");
        break;
      }

      ConnectionSetupPayload setupPayload;
      setupFrame.moveToSetupPayload(setupPayload);

      if (frameSerializer->protocolVersion() != setupPayload.protocolVersion) {
        transport->outputFrameOrEnqueue(frameSerializer->serializeOut(
            Frame_ERROR::badSetupFrame("invalid protocol version")));
        error("Invalid protocol version");
        break;
      }

      // Drop FrameProcessor before running ConnectionHandler callback.
      transport->setFrameProcessor(nullptr);

      connectionHandler->setupNewSocket(
          transport, std::move(setupPayload));

      erase(std::move(transport));
      break;
    }

    case FrameType::RESUME: {
      Frame_RESUME resumeFrame;
      if (!frameSerializer->deserializeFrom(resumeFrame, std::move(frame))) {
        transport->outputFrameOrEnqueue(
            frameSerializer->serializeOut(Frame_ERROR::invalidFrame()));
        error("Invalid RESUME frame");
        break;
      }

      ResumeParameters resumeParams(
          std::move(resumeFrame.token_),
          resumeFrame.lastReceivedServerPosition_,
          resumeFrame.clientPosition_,
          ProtocolVersion(
              resumeFrame.versionMajor_, resumeFrame.versionMinor_));

      if (frameSerializer->protocolVersion() != resumeParams.protocolVersion) {
        transport->outputFrameOrEnqueue(frameSerializer->serializeOut(
            Frame_ERROR::badSetupFrame("invalid protocol version")));
        error("Invalid protocol version");
        break;
      }

      // Drop FrameProcessor before running ConnectionHandler callback.
      transport->setFrameProcessor(nullptr);

      auto const triedResume =
          connectionHandler->resumeSocket(transport, std::move(resumeParams));
      if (!triedResume) {
        transport->outputFrameOrEnqueue(frameSerializer->serializeOut(
            Frame_ERROR::connectionError("cannot resume")));
        error("Resumption failed");
        break;
      }
      erase(std::move(transport));
      break;
    }

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
          frameSerializer->serializeOut(Frame_ERROR::unexpectedFrame()));
      error("Invalid frame received instead of SETUP/RESUME");
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void ServerConnectionAcceptor::accept(
    std::unique_ptr<DuplexConnection> connection,
    std::shared_ptr<ConnectionHandler> connectionHandler) {
  auto transport = std::make_shared<FrameTransport>(std::move(connection));
  auto processor = std::make_shared<OneFrameProcessor>(
      *this, transport, std::move(connectionHandler));

  {
    auto locked = connections_.lock();

    if (shutdown_) {
      return;
    }

    locked->insert(transport);
  }

  // Transport can receive frames right away.
  transport->setFrameProcessor(std::move(processor));
}

void ServerConnectionAcceptor::stop() {
  {
    auto locked = connections_.lock();

    shutdown_.emplace();

    if (locked->empty()) {
      return;
    }

    for (auto& connection : *locked) {
      connection->close(std::runtime_error("ServerConnectionAcceptor closed"));
    }
  }

  shutdown_->wait();
}

////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<FrameSerializer>
ServerConnectionAcceptor::getOrAutodetectFrameSerializer(
    const folly::IOBuf& firstFrame) {
  if (defaultFrameSerializer_) {
    return defaultFrameSerializer_;
  }

  auto serializer = FrameSerializer::createAutodetectedSerializer(firstFrame);
  if (!serializer) {
    LOG(ERROR) << "unable to detect protocol version";
    return nullptr;
  }

  VLOG(2) << "detected protocol version" << serializer->protocolVersion();
  return std::move(serializer);
}

void ServerConnectionAcceptor::closeAndRemoveConnection(
    const std::shared_ptr<ConnectionHandler>& connectionHandler,
    std::shared_ptr<FrameTransport> transport,
    folly::exception_wrapper ex) {
  transport->close(ex);
  connectionHandler->connectionError(transport, std::move(ex));

  erase(std::move(transport));
}

void ServerConnectionAcceptor::erase(
    std::shared_ptr<FrameTransport> transport) {
  auto locked = connections_.lock();

  auto result = locked->erase(transport);
  DCHECK_EQ(result, 1) << "Trying to remove connection that isn't tracked";

  if (shutdown_ && locked->empty()) {
    shutdown_->post();
  }
}

} // reactivesocket
