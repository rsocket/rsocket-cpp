// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>
#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "src/concurrent/OneToOneRingBuffer.h"

using namespace ::reactivesocket;
using namespace ::testing;
using namespace ::reactivesocket::RingBufferDescriptor;

using buffer_t = std::array<uint8_t, 64 * 1024 + TRAILER_LENGTH>;
using odd_sized_buffer_t = std::array<uint8_t, 64 * 1024 - 1 + TRAILER_LENGTH>;

constexpr static const std::int32_t MSG_TYPE_ID = 1;

class OneToOneRingBufferTest : public Test {
 public:
  OneToOneRingBufferTest()
      : rb_(buffer_.data(), static_cast<uint32_t>(buffer_.size())) {
    buffer_.fill(0);
    srcBuffer_.fill(5);
  }

 protected:
  REACTIVESOCKET_DECL_ALIGNED(buffer_t buffer_, 16);
  REACTIVESOCKET_DECL_ALIGNED(buffer_t srcBuffer_, 16);
  OneToOneRingBuffer rb_;
};

TEST_F(OneToOneRingBufferTest, shouldCalculateCapacityForBuffer) {
  EXPECT_EQ(rb_.capacity(), buffer_.size() - TRAILER_LENGTH);
}

TEST_F(OneToOneRingBufferTest, shouldThrowForCapacityNotPowerOfTwo) {
  REACTIVESOCKET_DECL_ALIGNED(odd_sized_buffer_t buffer, 16);
  buffer.fill(0);

  ASSERT_THROW(
      {
        OneToOneRingBuffer rb(
            buffer.data(), static_cast<uint32_t>(buffer.size()));
      },
      std::invalid_argument);
}

TEST_F(OneToOneRingBufferTest, shouldErrorWhenMaxMessageSizeExceeded) {
  EXPECT_EQ(
      rb_.write(MSG_TYPE_ID, srcBuffer_.data(), rb_.maxMsgLength() + 1),
      INVALID_ARGUMENT);
}

TEST_F(OneToOneRingBufferTest, shouldErrorWhenInvalidMsgTypeId) {
  EXPECT_EQ(rb_.write(-1, srcBuffer_.data(), 4), INVALID_ARGUMENT);
}

TEST_F(OneToOneRingBufferTest, shouldWriteToEmptyBuffer) {
  uint32_t length = 8;
  uint32_t alignedRecordLength = align(length + HEADER_LENGTH, ALIGNMENT);

  ASSERT_EQ(
      rb_.write(MSG_TYPE_ID, srcBuffer_.data(), length), alignedRecordLength);

  EXPECT_EQ(*(std::uint32_t*)(buffer_.data()), length + HEADER_LENGTH);
  EXPECT_EQ(*(std::int32_t*)(buffer_.data() + 4), MSG_TYPE_ID);

  const RingBufferDescriptorDefn* descriptor =
      (RingBufferDescriptorDefn*)(buffer_.data() + buffer_.size() - TRAILER_LENGTH);

  EXPECT_EQ(descriptor->tailPosition, alignedRecordLength);
}

TEST_F(OneToOneRingBufferTest, shouldRejectWriteWhenInsufficientSpace) {
  uint32_t length = 100;
  uint64_t tail = (rb_.capacity() - align(length - ALIGNMENT, ALIGNMENT));

  RingBufferDescriptorDefn* descriptor =
      (RingBufferDescriptorDefn*)(buffer_.data() + buffer_.size() - TRAILER_LENGTH);

  descriptor->headPosition = 0;
  descriptor->tailPosition = tail;

  ASSERT_EQ(
      rb_.write(MSG_TYPE_ID, srcBuffer_.data(), length), INSUFFICIENT_SPACE);

  EXPECT_EQ(descriptor->tailPosition, tail);
}

TEST_F(OneToOneRingBufferTest, shouldRejectWriteWhenBufferFull) {
  uint32_t length = 8;
  uint64_t tail = rb_.capacity();

  RingBufferDescriptorDefn* descriptor =
      (RingBufferDescriptorDefn*)(buffer_.data() + buffer_.size() - TRAILER_LENGTH);

  descriptor->headPosition = 0;
  descriptor->tailPosition = tail;

  ASSERT_EQ(
      rb_.write(MSG_TYPE_ID, srcBuffer_.data(), length), INSUFFICIENT_SPACE);

  EXPECT_EQ(descriptor->tailPosition, tail);
}

TEST_F(
    OneToOneRingBufferTest,
    shouldInsertPaddingRecordPlusMessageOnBufferWrap) {
  uint32_t length = 100;
  uint32_t recordLength = length + HEADER_LENGTH;
  uint32_t alignedRecordLength = align(recordLength, ALIGNMENT);
  uint64_t tail = rb_.capacity() - ALIGNMENT;
  uint64_t head = tail - (ALIGNMENT * 4);

  RingBufferDescriptorDefn* descriptor =
      (RingBufferDescriptorDefn*)(buffer_.data() + buffer_.size() - TRAILER_LENGTH);

  descriptor->headPosition = head;
  descriptor->tailPosition = tail;

  ASSERT_EQ(
      rb_.write(MSG_TYPE_ID, srcBuffer_.data(), length),
      tail + alignedRecordLength + ALIGNMENT);

  EXPECT_EQ(*(std::uint32_t*)(buffer_.data() + tail), ALIGNMENT);
  EXPECT_EQ(*(std::int32_t*)(buffer_.data() + tail + 4), PADDING_MSG_TYPE_ID);

  EXPECT_EQ(*(std::uint32_t*)(buffer_.data()), recordLength);
  EXPECT_EQ(*(std::int32_t*)(buffer_.data() + 4), MSG_TYPE_ID);

  EXPECT_EQ(descriptor->tailPosition, tail + alignedRecordLength + ALIGNMENT);
}

TEST_F(
    OneToOneRingBufferTest,
    shouldInsertPaddingRecordPlusMessageOnBufferWrapWithHeadEqualToTail) {
  uint32_t length = 100;
  uint32_t recordLength = length + HEADER_LENGTH;
  uint32_t alignedRecordLength = align(recordLength, ALIGNMENT);
  uint64_t tail = rb_.capacity() - ALIGNMENT;
  uint64_t head = tail;

  RingBufferDescriptorDefn* descriptor =
      (RingBufferDescriptorDefn*)(buffer_.data() + buffer_.size() - TRAILER_LENGTH);

  descriptor->headPosition = head;
  descriptor->tailPosition = tail;

  ASSERT_EQ(
      rb_.write(MSG_TYPE_ID, srcBuffer_.data(), length),
      tail + alignedRecordLength + ALIGNMENT);

  EXPECT_EQ(*(std::uint32_t*)(buffer_.data() + tail), ALIGNMENT);
  EXPECT_EQ(*(std::int32_t*)(buffer_.data() + tail + 4), PADDING_MSG_TYPE_ID);

  EXPECT_EQ(*(std::uint32_t*)(buffer_.data()), recordLength);
  EXPECT_EQ(*(std::int32_t*)(buffer_.data() + 4), MSG_TYPE_ID);

  EXPECT_EQ(descriptor->tailPosition, tail + alignedRecordLength + ALIGNMENT);
}

TEST_F(OneToOneRingBufferTest, shouldReadNothingFromEmptyBuffer) {
  uint32_t count = rb_.read(
      [&](std::int32_t id, const uint8_t* b, uint32_t length) {
        FAIL() << "should not receive anything";
      },
      1);

  EXPECT_EQ(count, 0u);
}

TEST_F(OneToOneRingBufferTest, shouldReadSingleMessage) {
  ASSERT_LT(rb_.write(MSG_TYPE_ID, srcBuffer_.data(), 7), UINT64_MAX - 1);

  size_t timesCalled = 0;
  uint32_t count = rb_.read(
      [&](std::int32_t id, const uint8_t* b, uint32_t length) {
        EXPECT_EQ(id, MSG_TYPE_ID);
        EXPECT_EQ(b, buffer_.data() + HEADER_LENGTH);
        EXPECT_EQ(length, 7u);
        timesCalled++;
      },
      10);

  EXPECT_EQ(count, 1u);
  EXPECT_EQ(timesCalled, 1u);
}

TEST_F(
    OneToOneRingBufferTest,
    shouldNotReadSingleMessagePartWayThroughWriting) {
  uint32_t length = 8;
  uint32_t recordLength = length + HEADER_LENGTH;
  uint32_t alignedRecordLength = align(recordLength, ALIGNMENT);
  uint64_t tail = alignedRecordLength;
  uint64_t head = 0;

  RingBufferDescriptorDefn* descriptor =
      (RingBufferDescriptorDefn*)(buffer_.data() + buffer_.size() - TRAILER_LENGTH);

  descriptor->tailPosition = tail;

  *(std::uint32_t*)buffer_.data() = 0;
  *(std::int32_t*)(buffer_.data() + 4) = MSG_TYPE_ID;

  uint32_t count = rb_.read(
      [&](std::int32_t id, const uint8_t* b, uint32_t length) {
        FAIL() << "should not receive anything";
      },
      1);

  EXPECT_EQ(count, 0u);

  EXPECT_EQ(descriptor->headPosition, head);
}

TEST_F(OneToOneRingBufferTest, shouldReadTwoMessages) {
  ASSERT_LT(rb_.write(MSG_TYPE_ID, srcBuffer_.data(), 7), UINT64_MAX - 1);
  ASSERT_LT(rb_.write(MSG_TYPE_ID, srcBuffer_.data(), 7), UINT64_MAX - 1);

  size_t timesCalled = 0;
  uint32_t count = rb_.read(
      [&](std::int32_t id, const uint8_t* b, uint32_t length) {
        EXPECT_EQ(id, MSG_TYPE_ID);
        EXPECT_EQ(length, 7u);
        timesCalled++;
      },
      10);

  EXPECT_EQ(count, 2u);
  EXPECT_EQ(timesCalled, 2u);
}

TEST_F(OneToOneRingBufferTest, shouldLimitReadOfMessages) {
  ASSERT_LT(rb_.write(MSG_TYPE_ID, srcBuffer_.data(), 7), UINT64_MAX - 1);
  ASSERT_LT(rb_.write(MSG_TYPE_ID, srcBuffer_.data(), 7), UINT64_MAX - 1);

  size_t timesCalled = 0;
  uint32_t count = rb_.read(
      [&](std::int32_t id, const uint8_t* b, uint32_t length) {
        EXPECT_EQ(id, MSG_TYPE_ID);
        EXPECT_EQ(length, 7u);
        timesCalled++;
      },
      1);

  EXPECT_EQ(count, 1u);
  EXPECT_EQ(timesCalled, 1u);
}

#define NUM_MESSAGES (1000 * 1000)
#define NUM_IDS_PER_THREAD (1000 * 1000)

TEST(OneToOneRingBufferConcurrentTest, shouldProvideCcorrelationIds) {
  REACTIVESOCKET_DECL_ALIGNED(buffer_t buffer, 16);
  buffer.fill(0);
  OneToOneRingBuffer rb(buffer.data(), static_cast<uint32_t>(buffer.size()));

  std::atomic<int> countDown(2);

  std::vector<std::thread> threads;

  for (int i = 0; i < 2; i++) {
    threads.push_back(std::thread([&]() {
      countDown--;
      while (countDown > 0) {
        std::this_thread::yield();
      }

      for (int m = 0; m < NUM_IDS_PER_THREAD; m++) {
        rb.nextCorrelationId();
      }
    }));
  }

  // wait for all threads to finish
  for (std::thread& thr : threads) {
    thr.join();
  }

  ASSERT_EQ(rb.nextCorrelationId(), NUM_IDS_PER_THREAD * 2);
}

TEST(OneToOneRingBufferConcurrentTest, shouldExchangeMessages) {
  REACTIVESOCKET_DECL_ALIGNED(buffer_t spscBuffer, 16);
  REACTIVESOCKET_DECL_ALIGNED(buffer_t srcBuffer, 16);
  spscBuffer.fill(0);
  srcBuffer.fill(0);
  OneToOneRingBuffer rb(
      spscBuffer.data(), static_cast<uint32_t>(spscBuffer.size()));

  std::atomic<bool> start{false};

  std::thread thread([&]() {
    start.store(true);

    for (std::uint32_t m = 0; m < NUM_MESSAGES; m++) {
      *((std::uint32_t*)srcBuffer.data()) = m;

      while (rb.write(MSG_TYPE_ID, srcBuffer.data(), 4) > (UINT64_MAX - 2)) {
        std::this_thread::yield();
      }
    }
  });

  while (!start) {
    std::this_thread::yield();
  }

  uint32_t msgCount = 0;
  uint32_t counts = 0;

  while (msgCount < NUM_MESSAGES) {
    const int readCount = rb.read(
        [&counts](
            std::int32_t msgTypeId, const uint8_t* buffer, uint32_t length) {
          const std::uint32_t messageNumber = *((std::uint32_t*)buffer);

          EXPECT_EQ(length, 4u);
          ASSERT_EQ(msgTypeId, MSG_TYPE_ID);

          ASSERT_EQ(counts, messageNumber);
          counts++;
        },
        UINT32_MAX);

    if (0 == readCount) {
      std::this_thread::yield();
    }

    msgCount += readCount;
  }

  thread.join();
}
