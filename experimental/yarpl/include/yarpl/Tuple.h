#pragma once

#include <atomic>
#include <iostream>

struct Tuple {
  const int a;
  const int b;

  Tuple(const int a, const int b) : a(a), b(b) {
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

  Tuple(Tuple&& t) : a(std::move(t.a)), b(std::move(t.b)) {
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
