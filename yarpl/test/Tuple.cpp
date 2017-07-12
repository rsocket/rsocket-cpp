// Copyright 2004-present Facebook. All Rights Reserved.

#include "Tuple.h"

namespace yarpl {

std::atomic<int> Tuple::createdCount;
std::atomic<int> Tuple::destroyedCount;
std::atomic<int> Tuple::instanceCount;
}
