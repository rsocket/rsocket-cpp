// Copyright 2004-present Facebook. All Rights Reserved.

#include "ServerConnectionAcceptor.h"
#include <folly/ExceptionWrapper.h>
#include "src/DuplexConnection.h"
#include "src/RSocketStats.h"
#include "src/framing/Frame.h"
#include "src/framing/FrameProcessor.h"
#include "src/framing/FrameSerializer.h"
#include "src/framing/FrameTransport.h"

#include <iostream>

namespace rsocket {

class OneFrameProcessor
    : public FrameProcessor,
      public std::enable_shared_from_this<OneFrameProcessor> {
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
    // no more code here as the instance might be gone by now
  }

  void onTerminal(folly::exception_wrapper ex) override {
    acceptor_.closeAndRemoveConnection(
        connectionHandler_, transport_, std::move(ex));
    // no more code here as the instance might be gone by now
  }

 private:
  ServerConnectionAcceptor& acceptor_;
  std::shared_ptr<FrameTransport> transport_;
  std::shared_ptr<ConnectionHandler> connectionHandler_;
};

ServerConnectionAcceptor::ServerConnectionAcceptor(
    ProtocolVersion protocolVersion) {
  // if protocolVersion is unknown we will try to autodetect the version
  // with the first frame
  if (protocolVersion != ProtocolVersion::Unknown) {
    defaultFrameSerializer_ =
        FrameSerializer::createFrameSerializer(protocolVersion);
  }
}

ServerConnectionAcceptor::~ServerConnectionAcceptor() {
  for (auto& connection : connections_) {
    connection->close(std::runtime_error("ServerConnectionAcceptor closed"));
  }
}

void ServerConnectionAcceptor::processFrame(
    std::shared_ptr<ConnectionHandler> connectionHandler,
    std::shared_ptr<FrameTransport> transport,
    std::unique_ptr<folly::IOBuf> frame) {
  auto frameSerializer = getOrAutodetectFrameSerializer(*frame);
  if (!frameSerializer) {
    closeAndRemoveConnection(
        connectionHandler,
        transport,
        std::runtime_error("Unable to detect protocol version"));
    return;
  }

  switch (frameSerializer->peekFrameType(*frame)) {
    case FrameType::SETUP: {
      Frame_SETUP setupFrame;
      if (!frameSerializer->deserializeFrom(setupFrame, std::move(frame))) {
        transport->outputFrameOrEnqueue(
            frameSerializer->serializeOut(Frame_ERROR::invalidFrame()));
        closeAndRemoveConnection(
            connectionHandler,
            std::move(transport),
            std::runtime_error("invalid"));
        break;
      }

      SetupParameters setupPayload;
      setupFrame.moveToSetupPayload(setupPayload);

      removeConnection(transport);

      if (frameSerializer->protocolVersion() != setupPayload.protocolVersion) {
        transport->outputFrameOrEnqueue(frameSerializer->serializeOut(
            Frame_ERROR::badSetupFrame("invalid protocol version")));
        transport->close(folly::exception_wrapper());
        break;
      }

      connectionHandler->setupNewSocket(
          std::move(transport), std::move(setupPayload));
      break;
    }

    case FrameType::RESUME: {
      Frame_RESUME resumeFrame;
      if (!frameSerializer->deserializeFrom(resumeFrame, std::move(frame))) {
        transport->outputFrameOrEnqueue(
            frameSerializer->serializeOut(Frame_ERROR::invalidFrame()));
        closeAndRemoveConnection(
            connectionHandler,
            std::move(transport),
            std::runtime_error("invalid"));
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
        closeAndRemoveConnection(
            connectionHandler,
            std::move(transport),
            std::runtime_error("invalid protocol version"));
        break;
      }

      removeConnection(transport);
      auto triedResume =
          connectionHandler->resumeSocket(transport, std::move(resumeParams));
      if (!triedResume) {
        transport->outputFrameOrEnqueue(frameSerializer->serializeOut(
            Frame_ERROR::connectionError("can not resume")));
        transport->close(std::runtime_error("can not resume"));
      }
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
      closeAndRemoveConnection(
          connectionHandler,
          std::move(transport),
          std::runtime_error("invalid"));
      break;
    }
  }
}

void ServerConnectionAcceptor::accept(
    std::unique_ptr<DuplexConnection> connection,
    std::shared_ptr<ConnectionHandler> connectionHandler) {
  auto transport = std::make_shared<FrameTransport>(std::move(connection));
  auto processor = std::make_shared<OneFrameProcessor>(
      *this, transport, std::move(connectionHandler));
  connections_.insert(transport);
  // transport can receive frames right away
  transport->setFrameProcessor(std::move(processor));
}

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
  connections_.erase(transport);
  connectionHandler->connectionError(std::move(transport), std::move(ex));
}

void ServerConnectionAcceptor::removeConnection(
    const std::shared_ptr<FrameTransport>& transport) {
  transport->setFrameProcessor(nullptr);
  connections_.erase(transport);
}

} // reactivesocket
