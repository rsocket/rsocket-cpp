// Copyright 2004-present Facebook. All Rights Reserved.

#include "SubscriptionRequester.h"

#include <algorithm>
#include <iostream>

namespace reactivesocket {

std::ostream& SubscriptionRequesterBase::logPrefix(std::ostream& os) {
  return os << "SubscriptionRequester(" << &connection_ << ", " << streamId_
            << "): ";
}
}
