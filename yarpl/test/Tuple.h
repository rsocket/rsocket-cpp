// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <iostream>

namespace yarpl {

struct Tuple {
  const int a;
  const int b;

  Tuple(const int _a, const int _b) : a(_a), b(_b) {
    std::cout << "Tuple (" << a << ", " << b << ") constructed." << std::endl;
    instanceCount++;
    createdCount++;
  }

  Tuple(const Tuple& t) : a(t.a), b(t.b) {
    std::cout << "Tuple (" << a << ", " << b << ") copy-constructed."
              << std::endl;
    instanceCount++;
    createdCount++;
  }

  Tuple(Tuple&& t) noexcept : a(std::move(t.a)), b(std::move(t.b)) {
    std::cout << "Tuple (" << a << ", " << b << ") move-constructed."
              << std::endl;
    instanceCount++;
    createdCount++;
  }

  ~Tuple() {
    std::cout << "Tuple (" << a << ", " << b << ") destroyed." << std::endl;
    std::cout << "Tuple destroyed!!" << std::endl;
    instanceCount--;
    destroyedCount++;
  }

  static std::atomic<int> createdCount;
  static std::atomic<int> destroyedCount;
  static std::atomic<int> instanceCount;
};

} // yarpl
