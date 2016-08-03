// Copyright 2004-present Facebook. All Rights Reserved.

#include "StreamRequester.h"

#include <algorithm>
#include <iostream>

namespace reactivesocket {

std::ostream& StreamRequesterBase::logPrefix(std::ostream& os) {
  return os << "StreamRequester(" << &connection_ << ", " << streamId_ << "): ";
}
}
