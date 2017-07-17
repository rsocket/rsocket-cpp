// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/SetupResumeAcceptor.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/async/EventBaseManager.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/RSocketStats.h"
#include "rsocket/framing/Frame.h"
#include "rsocket/framing/FrameProcessor.h"
#include "rsocket/framing/FrameSerializer.h"
#include "rsocket/framing/FrameTransport.h"

namespace rsocket {

class OneFrameProcessor : public FrameProcessor {
 public:
  OneFrameProcessor(
      SetupResumeAcceptor& acceptor,
      yarpl::Reference<FrameTransport> transport,
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
  yarpl::Reference<FrameTransport> transport_;
  SetupResumeAcceptor::OnSetup onSetup_;
  SetupResumeAcceptor::OnResume onResume_;
};

SetupResumeAcceptor::SetupResumeAcceptor(
    ProtocolVersion protocolVersion, folly::EventBase* eventBase)
    : eventBase_(eventBase) {
  // if protocolVersion is unknown we will try to autodetect the version
  // with the first frame
  if (protocolVersion != ProtocolVersion::Unknown) {
    defaultFrameSerializer_ =
        FrameSerializer::createFrameSerializer(protocolVersion);
  }
  CHECK(eventBase_);
}

SetupResumeAcceptor::~SetupResumeAcceptor() {
  close().get();
}

void SetupResumeAcceptor::processFrame(
    yarpl::Reference<FrameTransport> transport,
    std::unique_ptr<folly::IOBuf> frame,
    SetupResumeAcceptor::OnSetup onSetup,
    SetupResumeAcceptor::OnResume onResume) {
  DCHECK(eventBase_->isInEventBaseThread());

  if (closed_) {
    transport->closeWithError(std::runtime_error("shut down"));
    return;
  }

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

      if (!onResume(transport, std::move(resumeParams))) {
        transport->outputFrameOrEnqueue(frameSerializer->serializeOut(
            Frame_ERROR::rejectedResume("Resumption Failure")));
        // We are currently in FrameTransport's callstack where it would
        // have already acquired a recursive_mutex.  After the following block
        // of code, FrameTransport goes out of scope and will get destroyed. But
        // we cant destroy it while holding a recursive_mutex.  So schedule this
        // block of code to be executed after callstack has unwinded.
        eventBase_->runInEventBaseThread(
            [ this, transport = std::move(transport) ] {
              closeAndRemoveConnection(
                  std::move(transport),
                  std::runtime_error("Resumption Failure"));
            });
      } else {
        connections_.erase(transport);
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
  auto transport = yarpl::make_ref<FrameTransport>(std::move(connection));
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
    const yarpl::Reference<FrameTransport>& transport,
    folly::exception_wrapper ex) {
  if (ex) {
    transport->closeWithError(ex);
  } else {
    transport->close();
  }
  connections_.erase(transport);
}

void SetupResumeAcceptor::removeConnection(
    const yarpl::Reference<FrameTransport>& transport) {
  transport->setFrameProcessor(nullptr);
  connections_.erase(transport);
}

folly::Future<folly::Unit> SetupResumeAcceptor::close() {
  if(eventBase_->isInEventBaseThread()) {
    closeAllConnections();
    return folly::makeFuture();
  } else {
    return folly::via(eventBase_).then(
        [this]() mutable {
          closeAllConnections();
        });
  }
}

void SetupResumeAcceptor::closeAllConnections() {
  closed_ = true;
  for(auto& connection : connections_) {
    connection->closeWithError(std::runtime_error("shutting down"));
  }
}

} // reactivesocket
