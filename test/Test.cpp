#include <glog/logging.h>
#include "gmock/gmock.h"
#include <gflags/gflags.h>

int main(int argc, char **argv) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 100;

  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleMock(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  return RUN_ALL_TESTS();
}
