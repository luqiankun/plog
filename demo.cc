#include "plog.hpp"

int main(int argc, char** argv) {
  logger::initlogging(argv[0]);
  logger::colorlogtostderr(true);
  logger::logoutsink(LogSink::CONSOLE);
  // // logger::maxlogsize(1024 * 16);
  LOG(INFO) << "INFO" << "12" << std::endl;
  LOG(DEBUG) << "DEBUG" << std::endl;
  LOG(WARN) << "WARN";
  LOG_WARN("%s", "WARN");
  //   LOG_FATAL("%s%d", "12", 21);
  LOG(ERROR) << "hello world" << std::endl;
  LOG_IF(INFO, 1) << "hello world";
  for (int i = 0; i < 10; i++) {
    LOG_EVERY_N(INFO, 2) << "hello world every 2";
    LOG_ONCE(INFO) << "hello world once2";
  }
  std::thread t([]() {
    for (int i = 0; i < 10; i++) {
      LOG_EVERY_N(INFO, 2) << "hello world every 2";
      LOG_ONCE(INFO) << "hello world once2";
    }
  });
  t.join();
  logger::shudownlogging();
  return 0;
}
