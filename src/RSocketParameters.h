// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>
#include <string>
#include "src/Payload.h"
#include "src/framing/FrameSerializer.h"
#include "src/internal/Common.h"

namespace rsocket {

class RSocketParameters {
 public:
  RSocketParameters(bool _resumable, ProtocolVersion _protocolVersion)
      : resumable(_resumable), protocolVersion(std::move(_protocolVersion)) {}

  bool resumable;
  ProtocolVersion protocolVersion;
};

class SetupParameters : public RSocketParameters {
 public:
  explicit SetupParameters(
      std::string _metadataMimeType = "text/plain",
      std::string _dataMimeType = "text/plain",
      Payload _payload = Payload(),
      bool _resumable = false,
      const ResumeIdentificationToken& _token =
          ResumeIdentificationToken::generateNew(),
      ProtocolVersion _protocolVersion =
          FrameSerializer::getCurrentProtocolVersion())
      : RSocketParameters(_resumable, _protocolVersion),
        metadataMimeType(std::move(_metadataMimeType)),
        dataMimeType(std::move(_dataMimeType)),
        payload(std::move(_payload)),
        token(_token) {}

  std::string metadataMimeType;
  std::string dataMimeType;
  Payload payload;
  ResumeIdentificationToken token;
};

std::ostream& operator<<(std::ostream&, const SetupParameters&);

class ResumeParameters : public RSocketParameters {
 public:
  ResumeParameters(
      ResumeIdentificationToken _token,
      ResumePosition _serverPosition,
      ResumePosition _clientPosition,
      ProtocolVersion _protocolVersion)
      : RSocketParameters(true, _protocolVersion),
        token(std::move(_token)),
        serverPosition(_serverPosition),
        clientPosition(_clientPosition) {}

  ResumeIdentificationToken token;
  ResumePosition serverPosition;
  ResumePosition clientPosition;
};

} // reactivesocket
