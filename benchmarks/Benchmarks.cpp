// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Benchmark.h>
#include <folly/init/Init.h>

int main(int argc, char** argv) {
  folly::init(&argc, &argv);

  FLAGS_logtostderr = true;

  LOG(INFO) << "Running benchmarks... (takes minutes)";
  folly::runBenchmarks();

  return 0;
}
