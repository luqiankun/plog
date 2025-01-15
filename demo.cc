#include "plog.hpp"

int main(int argc, char** argv) {
  logger::initlogging(argv[0]);
  // logger::colorlogtostderr(true);
  logger::logoutsink(LogSink::LOGFILE);
  std::thread ths[100];
  std::atomic_uint64_t co{0};
  std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  for (int i = 0; i < 100; i++) {
    ths[i] = std::thread([&]() {
      for (int i = 0; i < 100; i++) {
        LOG(INFO) << "123 abc 啊拨次得额佛各 ＋＋×÷ α β γ ∴ m³ π "
                     "！@#$%……&*（）——+｛｝“：?><<OPPL:}{"
                     "udasdjqeldasncujsadlkasdlasdlasdasdwcvbnmbmsieieeee}"
                  << co++;
      }
    });
  }
  for (int i = 0; i < 100; i++) {
    ths[i].join();
  }
  // // // logger::maxlogsize(1024 * 16);
  // LOG(INFO) << "INFO" << "12" << std::endl;
  // LOG(DEBUG) << "DEBUG" << std::endl;
  // LOG(WARN) << "WARN";
  // LOG_WARN("%s", "WARN");
  // //   LOG_FATAL("%s%d", "12", 21);
  // LOG(ERROR) << "hello world" << std::endl;
  // LOG_IF(INFO, 1) << "hello world";
  // for (int i = 0; i < 10; i++) {
  //   LOG_EVERY_N(INFO, 2) << "hello world every 2";
  //   LOG_ONCE(INFO) << "hello world once2";
  // }
  // std::thread t([]() {
  //   for (int i = 0; i < 10; i++) {
  //     LOG_EVERY_N(INFO, 2) << "hello world every 2";
  //     LOG_ONCE(INFO) << "hello world once2";
  //   }
  // });
  // t.join();
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << std::endl;
  logger::shudownlogging();
  end = std::chrono::steady_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << std::endl;
  return 0;
}
