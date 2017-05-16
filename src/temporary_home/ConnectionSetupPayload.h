// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>
#include <string>
#include "src/internal/Common.h"
#include "src/framing/FrameSerializer.h"
#include "src/Payload.h"

namespace reactivesocket {

class SocketParameters {
 public:
  SocketParameters(bool _resumable, ProtocolVersion _protocolVersion)
      : resumable(_resumable), protocolVersion(std::move(_protocolVersion)) {}

  bool resumable;
  ProtocolVersion protocolVersion;
};

// TODO: rename this and the whole file to SetupParams
class ConnectionSetupPayload : public SocketParameters {
 public:
  explicit ConnectionSetupPayload(
      std::string _metadataMimeType = "",
      std::string _dataMimeType = "",
      Payload _payload = Payload(),
      bool _resumable = false,
      const ResumeIdentificationToken& _token =
          ResumeIdentificationToken::generateNew(),
      ProtocolVersion _protocolVersion =
          FrameSerializer::getCurrentProtocolVersion())
      : SocketParameters(_resumable, _protocolVersion),
        metadataMimeType(std::move(_metadataMimeType)),
        dataMimeType(std::move(_dataMimeType)),
        payload(std::move(_payload)),
        token(_token) {}

  std::string metadataMimeType;
  std::string dataMimeType;
  Payload payload;
  ResumeIdentificationToken token;
};

std::ostream& operator<<(std::ostream&, const ConnectionSetupPayload&);

class ResumeParameters : public SocketParameters {
 public:
  ResumeParameters(
      ResumeIdentificationToken _token,
      ResumePosition _serverPosition,
      ResumePosition _clientPosition,
      ProtocolVersion _protocolVersion)
      : SocketParameters(true, _protocolVersion),
        token(std::move(_token)),
        serverPosition(_serverPosition),
        clientPosition(_clientPosition) {}

  ResumeIdentificationToken token;
  ResumePosition serverPosition;
  ResumePosition clientPosition;
};

} // reactivesocket
