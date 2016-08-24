// Copyright 2004-present Facebook. All Rights Reserved.

#include "ConnectionSetupPayload.h"
#include <folly/String.h>

namespace reactivesocket {
std::ostream& operator<<(
    std::ostream& os,
    const ConnectionSetupPayload& setupPayload) {
  return os << "[metadataMimeType: " << setupPayload.metadataMimeType
            << " dataMimeType: " << setupPayload.dataMimeType
            << " payload: " << setupPayload.payload << "]";
}
}
