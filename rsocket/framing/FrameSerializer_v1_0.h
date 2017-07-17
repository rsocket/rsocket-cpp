// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/framing/FrameSerializer.h"

namespace rsocket {

class FrameSerializerV1_0 : public FrameSerializer {
 public:
  constexpr static const ProtocolVersion Version = ProtocolVersion(1, 0);
  constexpr static const size_t kFrameHeaderSize = 6; // bytes
  constexpr static const size_t kMinBytesNeededForAutodetection = 10; // bytes

  ProtocolVersion protocolVersion() override;

  static ProtocolVersion detectProtocolVersion(
      const folly::IOBuf& firstFrame,
      size_t skipBytes = 0);

  FrameType peekFrameType(const folly::IOBuf& in) override;
  folly::Optional<StreamId> peekStreamId(const folly::IOBuf& in) override;

  std::unique_ptr<folly::IOBuf> serializeOut(Frame_REQUEST_STREAM&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_REQUEST_CHANNEL&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_REQUEST_RESPONSE&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_REQUEST_FNF&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_REQUEST_N&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_METADATA_PUSH&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_CANCEL&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_PAYLOAD&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_ERROR&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_KEEPALIVE&&, bool) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_SETUP&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_LEASE&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_RESUME&&) override;
  std::unique_ptr<folly::IOBuf> serializeOut(Frame_RESUME_OK&&) override;

  bool deserializeFrom(Frame_REQUEST_STREAM&, std::unique_ptr<folly::IOBuf>)
      override;
  bool deserializeFrom(Frame_REQUEST_CHANNEL&, std::unique_ptr<folly::IOBuf>)
      override;
  bool deserializeFrom(Frame_REQUEST_RESPONSE&, std::unique_ptr<folly::IOBuf>)
      override;
  bool deserializeFrom(Frame_REQUEST_FNF&, std::unique_ptr<folly::IOBuf>)
      override;
  bool deserializeFrom(Frame_REQUEST_N&, std::unique_ptr<folly::IOBuf>)
      override;
  bool deserializeFrom(Frame_METADATA_PUSH&, std::unique_ptr<folly::IOBuf>)
      override;
  bool deserializeFrom(Frame_CANCEL&, std::unique_ptr<folly::IOBuf>) override;
  bool deserializeFrom(Frame_PAYLOAD&, std::unique_ptr<folly::IOBuf>) override;
  bool deserializeFrom(Frame_ERROR&, std::unique_ptr<folly::IOBuf>) override;
  bool deserializeFrom(Frame_KEEPALIVE&, std::unique_ptr<folly::IOBuf>, bool)
      override;
  bool deserializeFrom(Frame_SETUP&, std::unique_ptr<folly::IOBuf>) override;
  bool deserializeFrom(Frame_LEASE&, std::unique_ptr<folly::IOBuf>) override;
  bool deserializeFrom(Frame_RESUME&, std::unique_ptr<folly::IOBuf>) override;
  bool deserializeFrom(Frame_RESUME_OK&, std::unique_ptr<folly::IOBuf>)
      override;

  static std::unique_ptr<folly::IOBuf> deserializeMetadataFrom(
      folly::io::Cursor& cur,
      FrameFlags flags);
};
} // reactivesocket
