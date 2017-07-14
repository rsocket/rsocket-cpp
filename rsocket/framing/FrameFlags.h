// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstdint>
#include <iosfwd>

namespace rsocket {

enum class FrameFlags : uint16_t {
  EMPTY = 0x000,
  IGNORE = 0x200,
  METADATA = 0x100,

  // SETUP.
  RESUME_ENABLE = 0x80,
  LEASE = 0x40,

  // KEEPALIVE
  KEEPALIVE_RESPOND = 0x80,

  // REQUEST_RESPONSE, REQUEST_FNF, REQUEST_STREAM, REQUEST_CHANNEL, PAYLOAD.
  FOLLOWS = 0x80,

  // REQUEST_CHANNEL, PAYLOAD.
  COMPLETE = 0x40,

  // PAYLOAD.
  NEXT = 0x20,
};

constexpr uint16_t raw(FrameFlags flags) {
  return static_cast<uint16_t>(flags);
}

constexpr FrameFlags operator|(FrameFlags a, FrameFlags b) {
  return static_cast<FrameFlags>(raw(a) | raw(b));
}

constexpr FrameFlags operator&(FrameFlags a, FrameFlags b) {
  return static_cast<FrameFlags>(raw(a) & raw(b));
}

inline FrameFlags& operator|=(FrameFlags& a, FrameFlags b) {
  return a = (a | b);
}

inline FrameFlags& operator&=(FrameFlags& a, FrameFlags b) {
  return a = (a & b);
}

constexpr bool operator!(FrameFlags a) {
  return !raw(a);
}

constexpr FrameFlags operator~(FrameFlags a) {
  return static_cast<FrameFlags>(~raw(a));
}

std::ostream& operator<<(std::ostream&, FrameFlags);

}
