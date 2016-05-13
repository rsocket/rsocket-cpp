// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

namespace folly {
class IOBuf;
}

namespace lithium {
namespace reactivesocket {
/// The type of a read-only view on a binary buffer.
/// MUST manage the lifetime of the underlying buffer.
using Payload = std::unique_ptr<folly::IOBuf>;
}
}
