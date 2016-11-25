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
      const ResumeIdentificationToken& _token =
          {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}})
      : metadataMimeType(std::move(_metadataMimeType)),
        dataMimeType(std::move(_dataMimeType)),
        payload(std::move(_payload)),
        token(_token){};

  std::string metadataMimeType;
  std::string dataMimeType;
  Payload payload;
  const ResumeIdentificationToken token;
};

std::ostream& operator<<(std::ostream&, const ConnectionSetupPayload&);
}
