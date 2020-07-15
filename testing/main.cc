// apt install libgoogle-glog-dev
//#include <glog/logging.h>
// apt install libgtest-dev
#include <gtest/gtest.h>

int main(int argc, char* argv[]) {
  // ...
//  LOG(INFO) << "Initializing Google logging library";
  // Initialize Google's logging library.
//  google::InitGoogleLogging(argv[0]);

 // LOG(INFO) << "Initializing Google Test library";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
