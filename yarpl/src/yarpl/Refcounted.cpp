// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/Refcounted.h"

namespace yarpl {

#if !defined(NDEBUG)

Refcounted::Refcounted() {
  ++objects_;
}

Refcounted::~Refcounted() {
  --objects_;
}

size_t Refcounted::objects() {
  return objects_;
}

std::atomic_size_t Refcounted::objects_{0};

#endif /* !NDEBUG */

} // yarpl
