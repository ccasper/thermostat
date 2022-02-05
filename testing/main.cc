// apt install libgoogle-glog-dev
//#include <glog/logging.h>
// apt install libgtest-dev
#include <gtest/gtest.h>
// #include <glog/logging.h>

int main(int argc, char* argv[]) {
  LOG(INFO) << "Running Init Google";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
