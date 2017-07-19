// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/init/Init.h>
#include <glog/logging.h>
#include <gmock/gmock.h>

int main(int argc, char** argv) {
  FLAGS_logtostderr = true;
  testing::InitGoogleMock(&argc, argv);
  folly::init(&argc, &argv);
  return RUN_ALL_TESTS();
}
