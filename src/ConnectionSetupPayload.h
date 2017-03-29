// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>
#include <string>
#include "src/Payload.h"
#include "src/StreamState.h"

namespace reactivesocket {
class ConnectionSetupPayload {
 public:
  explicit ConnectionSetupPayload(
      std::string _metadataMimeType = "",
      std::string _dataMimeType = "",
      Payload _payload = Payload(),
      bool _resumable = false,
      const ResumeIdentificationToken& _token =
          ResumeIdentificationToken::generateNew())
      : metadataMimeType(std::move(_metadataMimeType)),
        dataMimeType(std::move(_dataMimeType)),
        payload(std::move(_payload)),
        token(_token),
        resumable(_resumable) {}

  std::string metadataMimeType;
  std::string dataMimeType;
  Payload payload;
  ResumeIdentificationToken token;
  bool resumable;
};

std::ostream& operator<<(std::ostream&, const ConnectionSetupPayload&);

class ResumeParameters {
 public:
  ResumeParameters(
      ResumeIdentificationToken _token,
      ResumePosition _serverPosition,
      ResumePosition _clientPosition)
      : token(std::move(_token)),
        serverPosition(_serverPosition),
        clientPosition(_clientPosition) {}

  ResumeIdentificationToken token;
  ResumePosition serverPosition;
  ResumePosition clientPosition;
};

} // reactivesocket
