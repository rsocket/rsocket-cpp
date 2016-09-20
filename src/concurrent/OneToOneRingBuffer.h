// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>

#include "src/concurrent/atomic64.h"

namespace reactivesocket {

namespace RingBufferDescriptor {

constexpr static const size_t kCACHE_LINE_LENGTH = 64;

#pragma pack(push)
#pragma pack(4)
struct RingBufferDescriptorDefn {
  std::uint8_t prePad[(2 * kCACHE_LINE_LENGTH)];
  std::uint64_t tailPosition;
  std::int8_t tailPositionPad[(2 * kCACHE_LINE_LENGTH) - sizeof(std::uint64_t)];
  std::uint64_t headCachePosition;
  std::int8_t
      headCachePositionPad[(2 * kCACHE_LINE_LENGTH) - sizeof(std::uint64_t)];
  std::uint64_t headPosition;
  std::int8_t headPositionPad[(2 * kCACHE_LINE_LENGTH) - sizeof(std::uint64_t)];
  std::uint64_t correlationCounter;
  std::int8_t
      correlationCounterPad[(2 * kCACHE_LINE_LENGTH) - sizeof(std::uint64_t)];
};
#pragma pack(pop)

constexpr static const uint32_t TRAILER_LENGTH =
    sizeof(RingBufferDescriptorDefn);
constexpr static const uint32_t HEADER_LENGTH = 2 * sizeof(std::int32_t);
constexpr static const std::int32_t PADDING_MSG_TYPE_ID = -1;
constexpr static const uint32_t ALIGNMENT = HEADER_LENGTH;
constexpr static const uint64_t INSUFFICIENT_SPACE = UINT64_MAX;
constexpr static const uint64_t INVALID_ARGUMENT = UINT64_MAX - 1;

template <typename T>
inline static T align(T value, T alignment) noexcept {
  return (value + (alignment - 1)) & ~(alignment - 1);
}

template <typename T>
inline static bool isPowerOfTwo(T value) noexcept {
  return value > 0 && ((value & (~value + 1)) == value);
}

inline static std::uint64_t makeHeader(
    std::uint32_t length,
    std::int32_t msgTypeId) {
  return ((static_cast<std::uint64_t>(msgTypeId) & 0xFFFFFFFF) << 32) |
      (length & 0xFFFFFFFF);
}

inline static std::uint32_t headerRecordLength(const std::uint64_t header) {
  return static_cast<std::uint32_t>(header);
}

inline static std::int32_t headerMessageTypeId(const std::uint64_t header) {
  return static_cast<std::int32_t>(header >> 32);
}
}

class OneToOneRingBuffer {
 public:
  using position_t = uint64_t;

  OneToOneRingBuffer(uint8_t* buffer, uint32_t length)
      : buffer_(buffer),
        capacity_(length - RingBufferDescriptor::TRAILER_LENGTH),
        descriptor_(
            reinterpret_cast<RingBufferDescriptor::RingBufferDescriptorDefn*>(
                buffer_ + capacity_)),
        tailPosition_(
            buffer_ + capacity_ +
            offsetof(
                RingBufferDescriptor::RingBufferDescriptorDefn,
                tailPosition)),
        headCachePosition_(
            buffer_ + capacity_ +
            offsetof(
                RingBufferDescriptor::RingBufferDescriptorDefn,
                headCachePosition)),
        headPosition_(
            buffer_ + capacity_ +
            offsetof(
                RingBufferDescriptor::RingBufferDescriptorDefn,
                headPosition)),
        correlationCounter_(
            buffer_ + capacity_ +
            offsetof(
                RingBufferDescriptor::RingBufferDescriptorDefn,
                correlationCounter)),
        maxMsgLength_(capacity_ / 8) {
    checkCapacity(capacity_);
  }

  OneToOneRingBuffer(const OneToOneRingBuffer&) = delete;
  OneToOneRingBuffer& operator=(const OneToOneRingBuffer&) = delete;

  inline uint32_t capacity() const noexcept {
    return capacity_;
  }

  inline uint32_t maxMsgLength() const noexcept {
    return maxMsgLength_;
  }

  position_t write(
      std::int32_t msgTypeId,
      const uint8_t* srcBuffer,
      uint32_t length) noexcept {
    if (msgTypeId < 1) {
      return RingBufferDescriptor::INVALID_ARGUMENT;
    }

    if (length > maxMsgLength_) {
      return RingBufferDescriptor::INVALID_ARGUMENT;
    }

    const uint32_t recordLength = length + RingBufferDescriptor::HEADER_LENGTH;
    const uint32_t requiredCapacity = RingBufferDescriptor::align(
        recordLength, RingBufferDescriptor::ALIGNMENT);
    const uint32_t mask = capacity_ - 1;

    std::uint64_t head = descriptor_->headCachePosition;
    std::uint64_t tail = descriptor_->tailPosition;
    const size_t availableCapacity = capacity_ - (tail - head);

    if (requiredCapacity > availableCapacity) {
      head = getUInt64Volatile(headPosition_);

      if (requiredCapacity > (capacity_ - (tail - head))) {
        return RingBufferDescriptor::INSUFFICIENT_SPACE;
      }

      descriptor_->headCachePosition = head;
    }

    uint32_t padding = 0;
    uint32_t recordIndex = static_cast<uint32_t>(tail) & mask;
    const uint32_t toBufferEndLength = capacity_ - recordIndex;

    if (requiredCapacity > toBufferEndLength) {
      uint32_t headIndex = static_cast<uint32_t>(head) & mask;

      if (requiredCapacity > headIndex) {
        head = getUInt64Volatile(headPosition_);
        headIndex = static_cast<uint32_t>(head) & mask;

        if (requiredCapacity > headIndex) {
          return RingBufferDescriptor::INSUFFICIENT_SPACE;
        }

        putUInt64Ordered(headCachePosition_, head);
      }

      padding = toBufferEndLength;
    }

    if (0 != padding) {
      putUInt64Ordered(
          buffer_ + recordIndex,
          RingBufferDescriptor::makeHeader(
              padding, RingBufferDescriptor::PADDING_MSG_TYPE_ID));
      recordIndex = 0;
    }

    ::memcpy(
        buffer_ + recordIndex + RingBufferDescriptor::HEADER_LENGTH,
        srcBuffer,
        length);
    putUInt64Ordered(
        buffer_ + recordIndex,
        RingBufferDescriptor::makeHeader(recordLength, msgTypeId));
    putUInt64Ordered(tailPosition_, tail + requiredCapacity + padding);

    return tail + requiredCapacity + padding;
  }

  template <typename F>
  uint32_t read(F&& handler, uint32_t messageCountLimit) noexcept {
    const std::uint64_t head = descriptor_->headPosition;
    const uint32_t headIndex = static_cast<uint32_t>(head) & (capacity_ - 1);
    const uint32_t contiguousBlockLength = capacity_ - headIndex;
    uint32_t messagesRead = 0;
    uint32_t bytesRead = 0;

    while ((bytesRead < contiguousBlockLength) &&
           (messagesRead < messageCountLimit)) {
      const uint32_t recordIndex = headIndex + bytesRead;
      const std::uint64_t header = getUInt64Volatile(buffer_ + recordIndex);
      const std::uint32_t recordLength =
          RingBufferDescriptor::headerRecordLength(header);

      if (recordLength <= 0) {
        break;
      }

      bytesRead += RingBufferDescriptor::align(
          recordLength, RingBufferDescriptor::ALIGNMENT);

      const std::int32_t msgTypeId =
          RingBufferDescriptor::headerMessageTypeId(header);
      if (RingBufferDescriptor::PADDING_MSG_TYPE_ID == msgTypeId) {
        continue;
      }

      ++messagesRead;
      handler(
          msgTypeId,
          buffer_ + recordIndex + RingBufferDescriptor::HEADER_LENGTH,
          recordLength - RingBufferDescriptor::HEADER_LENGTH);
    }

    if (bytesRead != 0) {
      ::memset(buffer_ + headIndex, 0, bytesRead);
      putUInt64Ordered(headPosition_, head + bytesRead);
    }

    return messagesRead;
  }

  inline std::int64_t nextCorrelationId() noexcept {
    return getAndAddUInt64(correlationCounter_, 1);
  }

 private:
  uint8_t* buffer_;
  uint32_t capacity_;

  RingBufferDescriptor::RingBufferDescriptorDefn* descriptor_;
  std::uint8_t* tailPosition_;
  std::uint8_t* headCachePosition_;
  std::uint8_t* headPosition_;
  std::uint8_t* correlationCounter_;
  uint32_t maxMsgLength_;

  inline static void checkCapacity(uint32_t capacity) {
    if (!RingBufferDescriptor::isPowerOfTwo(capacity)) {
      throw std::invalid_argument(
          "Capacity must be a positive power of 2 + TRAILER_LENGTH");
    }
  }
};
}
