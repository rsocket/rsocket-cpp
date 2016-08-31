// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

namespace reactivesocket {

/**
 * A compiler directive not reorder instructions.
 */
inline void thread_fence() {
  asm volatile("" ::: "memory");
}

/**
* Fence operation that uses locked addl as mfence is sometimes expensive
*/
inline void fence() {
  asm volatile("lock; addl $0,0(%%rsp)" : : : "cc", "memory");
}

inline void acquire() {
  volatile std::int64_t* dummy;
  asm volatile("movq 0(%%rsp), %0" : "=r"(dummy) : : "memory");
}

inline void release() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
  // Avoid hitting the same cache-line from different threads.
  volatile std::int64_t dummy = 0;
}
// must come after closing brace so that gcc knows it is actually unused
#pragma GCC diagnostic pop

/**
* A more jitter friendly alternate to thread:yield in spin waits.
*/
inline void cpu_pause() {
  asm volatile("pause\n" : : : "memory");
}

inline std::uint64_t getUInt64Volatile(volatile std::uint8_t* source) {
  uint64_t sequence = *reinterpret_cast<volatile std::uint64_t*>(source);
  thread_fence();
  return sequence;
}

inline void putUInt64Ordered(
    volatile std::uint8_t* address,
    std::uint64_t value) {
  thread_fence();
  *reinterpret_cast<volatile std::uint64_t*>(address) = value;
}

inline std::uint64_t getAndAddUInt64(
    volatile std::uint8_t* address,
    std::uint64_t value) {
  std::uint64_t original;
  asm volatile("lock; xaddq %0, %1"
               : "=r"(original), "+m"(*address)
               : "0"(value));
  return original;
}

//-------------------------------------
//  Alignment
//-------------------------------------
// Note: May not work on local variables.
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=24691
#define REACTIVESOCKET_DECL_ALIGNED(declaration, amt) \
  declaration __attribute__((aligned(amt)))
}
