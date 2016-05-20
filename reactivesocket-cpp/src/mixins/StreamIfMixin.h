// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "reactivesocket-cpp/src/AbstractStreamAutomaton.h"
#include "reactivesocket-cpp/src/Payload.h"

namespace reactivesocket {

/// A mixin which provides dynamic dispatch for a chain of mixins and
/// presents the chain as an AbstractStreamAutomaton.
///
/// For performance and memory consumption reasons this mixin should be the last
/// one in the chain (as enforced by 'final' specifiers). Moving it up the chain
/// introduces vtable pointers in ever link of the chain that follows this
/// mixin.
template <typename Base>
class StreamIfMixin : public Base, public AbstractStreamAutomaton {
 public:
  using Base::Base;

  void endStream(StreamCompletionSignal signal) override final {
    Base::endStream(signal);
  }

  void onNextFrame(Frame_REQUEST_SUB& frame) override final {
    Base::onNextFrame(frame);
  }
  void onNextFrame(Frame_REQUEST_CHANNEL& frame) override final {
    Base::onNextFrame(frame);
  }
  void onNextFrame(Frame_REQUEST_N& frame) override final {
    Base::onNextFrame(frame);
  }
  void onNextFrame(Frame_CANCEL& frame) override final {
    Base::onNextFrame(frame);
  }
  void onNextFrame(Frame_RESPONSE& frame) override final {
    Base::onNextFrame(frame);
  }
  void onNextFrame(Frame_ERROR& frame) override final {
    Base::onNextFrame(frame);
  }

  void onBadFrame() override final {
    Base::onBadFrame();
  }
};
}
