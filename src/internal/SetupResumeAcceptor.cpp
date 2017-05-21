// Copyright 2004-present Facebook. All Rights Reserved.

#include "SetupResumeAcceptor.h"
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
      SetupResumeAcceptor& acceptor,
      std::shared_ptr<FrameTransport> transport,
      SetupResumeAcceptor::OnSetup onSetup,
      SetupResumeAcceptor::OnResume onResume)
      : acceptor_(acceptor),
        transport_(std::move(transport)),
        onSetup_(std::move(onSetup)),
        onResume_(std::move(onResume)) {
    DCHECK(transport_);
    DCHECK(onSetup_);
    DCHECK(onResume_);
  }

  void processFrame(std::unique_ptr<folly::IOBuf> buf) override {
    acceptor_.processFrame(std::move(transport_), std::move(buf),
                           std::move(onSetup_), std::move(onResume_));
    // no more code here as the instance might be gone by now
  }

  void onTerminal(folly::exception_wrapper ex) override {
    onSetup_ = nullptr;
    onResume_ = nullptr;

    acceptor_.closeAndRemoveConnection(
        std::move(transport_), std::move(ex));
    // no more code here as the instance might be gone by now
  }

 private:
  SetupResumeAcceptor& acceptor_;
  std::shared_ptr<FrameTransport> transport_;
  SetupResumeAcceptor::OnSetup onSetup_;
  SetupResumeAcceptor::OnResume onResume_;
};

SetupResumeAcceptor::SetupResumeAcceptor(
    ProtocolVersion protocolVersion) {
  // if protocolVersion is unknown we will try to autodetect the version
  // with the first frame
  if (protocolVersion != ProtocolVersion::Unknown) {
    defaultFrameSerializer_ =
        FrameSerializer::createFrameSerializer(protocolVersion);
  }
}

SetupResumeAcceptor::~SetupResumeAcceptor() {
  for (auto& connection : connections_) {
    connection->close(std::runtime_error("SetupResumeAcceptor closed"));
  }
}

void SetupResumeAcceptor::processFrame(
    std::shared_ptr<FrameTransport> transport,
    std::unique_ptr<folly::IOBuf> frame,
    SetupResumeAcceptor::OnSetup onSetup,
    SetupResumeAcceptor::OnResume onResume) {
  auto frameSerializer = getOrAutodetectFrameSerializer(*frame);
  if (!frameSerializer) {
    closeAndRemoveConnection(
        std::move(transport),
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
            std::move(transport),
            std::runtime_error("invalid"));
        break;
      }

      SetupParameters setupPayload;
      setupFrame.moveToSetupPayload(setupPayload);

      if (frameSerializer->protocolVersion() != setupPayload.protocolVersion) {
        transport->outputFrameOrEnqueue(frameSerializer->serializeOut(
            Frame_ERROR::badSetupFrame("invalid protocol version")));
        closeAndRemoveConnection(
            transport,
            std::runtime_error("invalid protocol version for resume"));
        break;
      }

      removeConnection(transport);
      onSetup(std::move(transport), std::move(setupPayload));
      break;
    }

    case FrameType::RESUME: {
      Frame_RESUME resumeFrame;
      if (!frameSerializer->deserializeFrom(resumeFrame, std::move(frame))) {
        transport->outputFrameOrEnqueue(
            frameSerializer->serializeOut(Frame_ERROR::invalidFrame()));
        closeAndRemoveConnection(
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
            std::move(transport),
            std::runtime_error("invalid protocol version"));
        break;
      }

      removeConnection(transport);
      onResume(std::move(transport), std::move(resumeParams));
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
          std::move(transport),
          std::runtime_error("invalid"));
      break;
    }
  }
}

void SetupResumeAcceptor::accept(
    std::unique_ptr<DuplexConnection> connection,
    OnSetup onSetup,
    OnResume onResume) {
  auto transport = std::make_shared<FrameTransport>(std::move(connection));
  auto processor = std::make_shared<OneFrameProcessor>(
      *this, transport, std::move(onSetup), std::move(onResume));
  connections_.insert(transport);
  // transport can receive frames right away
  transport->setFrameProcessor(std::move(processor));
}

std::shared_ptr<FrameSerializer>
SetupResumeAcceptor::getOrAutodetectFrameSerializer(
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

void SetupResumeAcceptor::closeAndRemoveConnection(
    const std::shared_ptr<FrameTransport>& transport,
    folly::exception_wrapper ex) {
  transport->close(ex);
  connections_.erase(transport);
}

void SetupResumeAcceptor::removeConnection(
    const std::shared_ptr<FrameTransport>& transport) {
  transport->setFrameProcessor(nullptr);
  connections_.erase(transport);
}

} // reactivesocket
