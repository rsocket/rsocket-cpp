// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/portability/GFlags.h>

#include <glog/logging.h>
#include "gmock/gmock.h"

int main(int argc, char** argv) {
  FLAGS_logtostderr = true;
  ::testing::InitGoogleMock(&argc, argv);
#ifdef OSS
  google::ParseCommandLineFlags(&argc, &argv, true);
#else
  gflags::ParseCommandLineFlags(&argc, &argv, true);
#endif
  google::InitGoogleLogging(argv[0]);
  return RUN_ALL_TESTS();
}
