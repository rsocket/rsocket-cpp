// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>

#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace lithium {
namespace reactivesocket {

/// A mixin which provides dynamic dispatch for a chain of mixins and
/// presents the chain as a Subscriber<Payload>.
///
/// For performance and memory consumption reasons this mixin should be the last
/// one in the chain (as enforced by 'final' specifiers). Moving it up the chain
/// introduces vtable pointers in ever link of the chain that follows this
/// mixin.
template <typename Base>
class SinkIfMixin : public Base, public Subscriber<Payload> {
 public:
  using Base::Base;

  void onSubscribe(Subscription& subscription) override final {
    Base::onSubscribe(subscription);
  }

  void onNext(Payload element) override final {
    Base::onNext(std::move(element));
  }

  void onComplete() override final {
    Base::onComplete();
  }

  void onError(folly::exception_wrapper ex) override final {
    Base::onError(std::move(ex));
  }
};
}
}
