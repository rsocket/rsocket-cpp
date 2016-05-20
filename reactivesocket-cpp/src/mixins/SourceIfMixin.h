// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace reactivesocket {

/// A mixin which provides dynamic dispatch for a chain of mixins and
/// presents the chain as a Subscription.
///
/// For performance and memory consumption reasons this mixin should be the last
/// one in the chain (as enforced by 'final' specifiers). Moving it up the chain
/// introduces vtable pointers in ever link of the chain that follows this
/// mixin.
template <typename Base>
class SourceIfMixin : public Base, public Subscription {
 public:
  using Base::Base;

  void request(size_t n) override final {
    Base::request(n);
  }

  void cancel() override final {
    Base::cancel();
  }
};
}
