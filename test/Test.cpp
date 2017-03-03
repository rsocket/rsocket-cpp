// Copyright 2004-present Facebook. All Rights Reserved.

#include <gflags/gflags.h>
#include <glog/logging.h>
#include "gmock/gmock.h"

int main(int argc, char** argv) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 100;

  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleMock(&argc, argv);
#ifdef OSS
  google::ParseCommandLineFlags(&argc, &argv, true);
#else
  gflags::ParseCommandLineFlags(&argc, &argv, true);
#endif
  return RUN_ALL_TESTS();
}
