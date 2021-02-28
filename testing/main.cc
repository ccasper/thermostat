// apt install libgoogle-glog-dev
//#include <glog/logging.h>
// apt install libgtest-dev
#include <gtest/gtest.h>

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
