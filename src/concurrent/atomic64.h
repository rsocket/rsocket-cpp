// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

/*
 * Determine platform, compiler, and CPU and set defines to be used later.
 * Also, error out here if on a platform that is not supported.
 */

#if defined(__GNUC__)
#define REACTIVESOCKET_COMPILER_GCC 1

#if defined(__llvm__)
#define REACTIVESOCKET_COMPILER_LLVM 1
#endif

#if defined(__x86_64__)
#define REACTIVESOCKET_CPU_X64 1

#else
#error Unsupported CPU!
#endif

#else
#error Unsupported compiler!
#endif

#include <cstdint>

#if defined(REACTIVESOCKET_COMPILER_GCC) && defined(REACTIVESOCKET_CPU_X64)
#include <src/concurrent/atomic64_gcc_x86_64.h>

#else
#error Unsupported platform!
#endif
