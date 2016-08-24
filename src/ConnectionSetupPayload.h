// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>
#include <string>
#include "Payload.h"

namespace reactivesocket {
class ConnectionSetupPayload {
 public:
  ConnectionSetupPayload(
      std::string _metadataMimeType = "",
      std::string _dataMimeType = "",
      Payload _payload = Payload())
      : metadataMimeType(std::move(_metadataMimeType)),
        dataMimeType(std::move(_dataMimeType)),
        payload(std::move(_payload)){};

  std::string metadataMimeType;
  std::string dataMimeType;
  Payload payload;
};

std::ostream& operator<<(std::ostream&, const ConnectionSetupPayload&);
}
