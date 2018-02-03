#pragma once

#include "rsocket/Payload.h"
#include "rsocket/framing/Frame.h"
#include "rsocket/framing/FrameHeader.h"

namespace rsocket {

struct StreamFragmentAccumulator {
  FrameHeader header;
  uint32_t requestN{0};
  Payload fragments;

  StreamFragmentAccumulator(StreamFragmentAccumulator const&) = delete;
  StreamFragmentAccumulator(StreamFragmentAccumulator&&) = default;

  StreamFragmentAccumulator() {}

  explicit StreamFragmentAccumulator(
      Frame_REQUEST_Base const& frame,
      Payload payload)
      : StreamFragmentAccumulator(
            frame.header_,
            frame.requestN_,
            std::move(payload)) {}

  explicit StreamFragmentAccumulator(
      Frame_REQUEST_RESPONSE const& frame,
      Payload payload)
      : StreamFragmentAccumulator(frame.header_, 0, std::move(payload)) {}

 private:
  explicit StreamFragmentAccumulator(
      FrameHeader const& fh,
      uint32_t requestN,
      Payload payload)
      : header(fh), requestN(requestN) {
    addPayload(std::move(payload));
  }

 public:
  void addPayload(Payload p) {
    if (p.metadata) {
      if (!fragments.metadata) {
        fragments.metadata = std::move(p.metadata);
      } else {
        fragments.metadata->prev()->appendChain(std::move(p.metadata));
      }
    }

    if (p.data) {
      if (!fragments.data) {
        fragments.data = std::move(p.data);
      } else {
        fragments.data->prev()->appendChain(std::move(p.data));
      }
    }
  }

  Payload consumePayload() {
    return std::move(fragments);
  }

  bool anyFragments() const {
    return fragments.data || fragments.metadata;
  }
};

} /* namespace rsocket */