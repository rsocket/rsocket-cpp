// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/init/Init.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "yarpl/Refcounted.h"

int main(int argc, char** argv) {
  int ret;
  {
    FLAGS_logtostderr = true;
    ::testing::InitGoogleTest(&argc, argv);
    folly::init(&argc, &argv);
    ret = RUN_ALL_TESTS();
  }

  return ret;
}
