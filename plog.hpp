#ifndef LOG_GING_HPP
#define LOG_GING_HPP
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
enum LogLevel {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
  FATAL = 4,
};
enum LogSink {
  CONSOLE = 0,           // 控制台
  LOGFILE = 1,           // 日志文件
  CONSOLE_AND_FILE = 2,  // 控制台和日志文件
};
namespace logger {
inline thread_local size_t thread_id_hash =
    std::hash<std::thread::id>{}(std::this_thread::get_id());
inline static char data_buf[std::size("20200101-00:00:00")];
inline static timespec last_time;
inline static bool first_log{true};
inline const char* COLOR_RED = "\033[31m";
inline const char* COLOR_GREEN = "\033[32m";
inline const char* COLOR_YELLOW = "\033[33m";
inline const char* COLOR_BLUE = "\033[34m";
inline const char* COLOR_MAGENTA = "\033[35m";
inline const char* COLOR_CYAN = "\033[36m";
inline const char* COLOR_RESET = "\033[0m";
void maxchahesize(uint64_t t);  // 持久化缓冲区大小,默认1024*8
void minloglevel(int);          // 设置日志级别，默认INFO
void colorlogtostderr(bool);    // 设置是否输出颜色，默认false
void maxlogsize(uint64_t);      // 设置日志文件分卷大小，默认1024*1024*10
void logoutsink(int sink);      // 设置日志输出位置，默认CONSOLE
void maxrollsize(uint64_t);     // 设置日志文件分卷数量
void initlogging(
    const char* argv_0);  // 初始化日志 默认日志文件存放路径为当前目录
void initlogging(const char* argv_0,
                 const char* file_path);  // 初始化日志 日志文件存放路径
void shudownlogging();                    // 关闭日志,退出前调用
std::string level_str(LogLevel level);
class LogEntry {
 public:
  LogEntry() = delete;
  LogEntry(int l, LogLevel lv, const std::string& file, const char* f,
           size_t tid, const char* time, const std::string& msg)
      : line(l),
        level(lv),
        file_name(file),
        func(f),
        thread_id(tid),
        time(time),
        msg(msg) {}
  int line;
  LogLevel level;
  std::string file_name;
  std::string func;
  size_t thread_id;
  std::string time;
  std::string msg;
};
class LogStream : public std::ostringstream {};
class LogBuffer {
 public:
  const size_t max_size{1024};
  enum class Status { Open = 0, Full = 1 };
  bool push(const LogEntry& entry) {
    if (status == Status::Full) {
      return false;
    }
    log_queue.push_back(entry);
    if (log_queue.size() > max_size) {
      status = Status::Full;
    }
    return true;
  };
  void set_statue(Status st) { status = st; }
  bool empty() { return log_queue.empty(); }
  std::vector<uint8_t> data();

 private:
  std::deque<LogEntry> log_queue;
  Status status{Status::Open};
};

class Logger {
 public:
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  void shutdown() {
    run = false;
    flush();
    if (flush_thread.joinable()) flush_thread.join();
    if (ofs.is_open()) {
      ofs.flush();
      ofs.close();
    }
  }
  static std::shared_ptr<Logger> get_instance() {
    std::call_once(init_flag, &Logger::initialize);
    return instance;
  }
  void operator()(const LogEntry& entry) {
    std::unique_lock<std::mutex> lock(buf_mut);
    if (cur_in_buf->push(entry)) {
    } else {
      auto temp = std::make_shared<LogBuffer>();
      log_buffers.push_front(temp);
      cur_in_buf = temp;
      cur_in_buf->push(entry);
      flush();
    }
  }
  void flush() { cv.notify_one(); }

 private:
  Logger() : log_max_size(1024 * 1024 * 10), log_cache_size(1024 * 8) {
    log_buffers.emplace_front(std::make_shared<LogBuffer>());
    cur_in_buf = log_buffers.front();
    cur_out_buf = cur_in_buf;
    log_std_out = true;
  }
  void start();
  static void initialize() {
    instance = std::shared_ptr<Logger>(new Logger());
    instance->start();
  }

 private:
  std::ofstream ofs;
  bool run{true};
  std::list<std::shared_ptr<LogBuffer>> log_buffers;
  std::shared_ptr<LogBuffer> cur_in_buf;
  std::shared_ptr<LogBuffer> cur_out_buf;
  static std::shared_ptr<Logger> instance;
  static std::once_flag init_flag;
  std::condition_variable cv;
  std::mutex wait_mut;
  std::mutex buf_mut;
  std::thread flush_thread;

 public:
  std::string out_file;
  uint64_t log_max_size;
  uint64_t log_cache_size;
  uint32_t log_max_roll{5};
  bool log_std_out{true};
  bool log_file_out{false};
  bool log_std_color{false};
  bool log_simple_out{true};
  LogLevel log_min_level{INFO};
  std::atomic_bool fatal{false};
};
inline std::shared_ptr<Logger> Logger::instance = nullptr;
inline std::once_flag Logger::init_flag;

class LogHolder {
 public:
  LogHolder() = delete;
  LogHolder(std::string file_name, int line, LogLevel level, const char* func);
  ~LogHolder() {
    if (level < Logger::get_instance()->log_min_level) return;
    LogEntry entry = LogEntry{line,           level,     file_name, func,
                              thread_id_hash, this_time, ss.str()};
    Logger::get_instance()->operator()(entry);
  }

 public:
  LogStream ss;

 private:
  char this_time[26];
  std::string file_name;
  int line;
  LogLevel level{INFO};
  const char* func;
};

////common
inline void logoutsink(LogSink sink) {
  if (sink == LogSink::CONSOLE) {
    Logger::get_instance()->log_std_out = true;
    Logger::get_instance()->log_file_out = false;
  } else if (sink == LogSink::LOGFILE) {
    Logger::get_instance()->log_std_out = false;
    Logger::get_instance()->log_file_out = true;
  } else {
    Logger::get_instance()->log_std_out = true;
    Logger::get_instance()->log_file_out = true;
  }
}
inline void minloglevel(int level) {
  Logger::get_instance()->log_min_level = (LogLevel)level;
}
inline void maxrollsize(uint64_t s) {
  Logger::get_instance()->log_max_roll = s;
}

inline void colorlogtostderr(bool f) {
  Logger::get_instance()->log_std_color = f;
}
inline void maxlogsize(uint64_t t) { Logger::get_instance()->log_max_size = t; }
inline void maxchahesize(uint64_t t) {
  Logger::get_instance()->log_cache_size = t;
}
inline void initlogging(const char* argv_0) {
  Logger::get_instance()->out_file = argv_0;
  Logger::get_instance()->out_file += ".log";
}
inline void initlogging(const char* argv_0, const char* file_path) {
  std::string file_name(argv_0);
  auto found = file_name.find_last_of("/\\");
  if (found != std::string::npos) {
    file_name = file_name.substr(found + 1);
  }
  Logger::get_instance()->out_file =
      std::string(file_path) + "/" + file_name + ".log";
}
inline std::string level_str(LogLevel level) {
  switch (level) {
    case DEBUG:
      return "DEBUG";
    case INFO:
      return "INFO";
    case WARN:
      return "WARN";
    case ERROR:
      return "ERROR";
    case FATAL:
      return "FATAL";
    default:
      return "DEBUG";
  }
}
//////

/// logholder
inline LogHolder::LogHolder(std::string file_name, int line, LogLevel level,
                            const char* func)
    : file_name(file_name), line(line), level(level), func(func) {
  if (first_log) {
    first_log = false;
    timespec_get(&last_time, TIME_UTC);
    std::strftime(data_buf, sizeof(data_buf), "%Y%m%d %H:%M:%S",
                  std::localtime(&last_time.tv_sec));
    uint32_t ms = last_time.tv_nsec / 1000;
    memcpy(this_time, data_buf, sizeof(data_buf));
    snprintf(this_time + 17, 9, "%c%06dZ", '.', ms);
  } else {
    std::timespec ts;
    timespec_get(&ts, TIME_UTC);
    if (ts.tv_sec == last_time.tv_sec) {
      // 同一秒
      uint32_t ms = ts.tv_nsec / 1000;
      memcpy(this_time, data_buf, sizeof(data_buf));
      snprintf(this_time + 17, 9, "%c%06dZ", '.', ms);
    } else {
      // 不是同一秒
      std::strftime(data_buf, sizeof(data_buf), "%Y%m%d %H:%M:%S",
                    std::localtime(&last_time.tv_sec));
      uint32_t ms = ts.tv_nsec / 1000;
      memcpy(this_time, data_buf, sizeof(data_buf));
      snprintf(this_time + 17, 9, "%c%06dZ", '.', ms);
    }
    last_time = ts;
  }
}
///
inline void shudownlogging() { Logger::get_instance()->shutdown(); }
inline void Logger::start() {
  static bool first{true};
  if (first) {
    flush_thread = std::thread([&]() {
      while (run) {
        if (log_file_out && !ofs.is_open()) {
          ofs = std::ofstream(out_file, std::ios::out | std::ios::app);
        }
        std::unique_lock<std::mutex> lock(wait_mut);
        cv.wait_for(lock, std::chrono::milliseconds(100));
        std::unique_lock<std::mutex> lck(buf_mut);
        if (!cur_in_buf->empty()) {
          cur_in_buf->set_statue(LogBuffer::Status::Full);
          auto temp = std::make_shared<LogBuffer>();
          log_buffers.push_front(temp);
          cur_in_buf = log_buffers.front();
        }
        lck.unlock();
        uint64_t cache_size{0};
        while (cur_in_buf != cur_out_buf) {
          auto msg = cur_out_buf->data();
          log_buffers.pop_back();
          cur_out_buf = log_buffers.back();
          if (!log_file_out) break;
          if (!ofs.is_open()) break;
          size_t w_ok_len = 0;
          size_t cur_w_index = 0;
          while (w_ok_len < msg.size()) {
            size_t need_w_len = msg.size() - w_ok_len;
            if (need_w_len + ofs.tellp() <= log_max_size) {
              // 可以写入
              ofs.write((char*)msg.data() + cur_w_index, need_w_len);
              if (cache_size >= log_cache_size) {
                cache_size = 0;
                ofs.flush();
              }
              w_ok_len += need_w_len;
              cur_w_index += need_w_len;
            } else {
              // roll
              size_t vaild_len = log_max_size - ofs.tellp();
              w_ok_len += vaild_len;
              ofs.write((char*)msg.data() + cur_w_index, vaild_len);
              if (cache_size >= log_cache_size) {
                cache_size = 0;
                ofs.flush();
              }
              cur_w_index += vaild_len;
              // 分卷
              ofs.close();
              for (int i = log_max_roll - 1; i >= 1; i--) {
                std::string old_name = out_file + "." + std::to_string(i);
                std::string ne_name = out_file + "." + std::to_string(i + 1);
                std::rename(old_name.c_str(), ne_name.c_str());
              }
              std::string new_name = out_file + "." + std::to_string(1);
              std::rename(out_file.c_str(), new_name.c_str());
              ofs = std::ofstream(out_file, std::ios::out | std::ios::app);
            }
          }
          ofs.flush();
          if (fatal) {
            ofs.close();
            abort();
          }
        }
      }
    });
    first = false;
  }
}

inline std::vector<uint8_t> LogBuffer::data() {
  std::vector<uint8_t> res;
  while (!log_queue.empty()) {
    auto top = log_queue.front();
    log_queue.pop_front();
    if (Logger::get_instance()->log_std_out) {
      if (!Logger::get_instance()->log_std_color) {
        if (Logger::get_instance()->log_simple_out) {
          printf("%-5s %s %16zx %s:%-d]%s\n", level_str(top.level).c_str(),
                 top.time.c_str(), top.thread_id, top.file_name.c_str(),
                 top.line, top.msg.c_str());
        } else {
          printf("%-5s %s %16zx %s:%-d %s()]%s\n", level_str(top.level).c_str(),
                 top.time.c_str(), top.thread_id, top.file_name.c_str(),
                 top.line, top.func.c_str(), top.msg.c_str());
        }
      } else {
        const char* color{COLOR_GREEN};
        switch (top.level) {
          case DEBUG:
            color = COLOR_CYAN;
            break;
          case INFO:
            color = COLOR_GREEN;
            break;
          case WARN:
            color = COLOR_YELLOW;
            break;
          case ERROR:
            color = COLOR_RED;
            break;
          case FATAL:
            color = COLOR_MAGENTA;
            break;
        }
        if (Logger::get_instance()->log_simple_out) {
          printf("%s%-5s %s %16zx %s:%-d]%s%s\n", color,
                 level_str(top.level).c_str(), top.time.c_str(), top.thread_id,
                 top.file_name.c_str(), top.line, top.msg.c_str(), COLOR_RESET);
        } else {
          printf("%s%-5s %s %16zx %s:%-d %s()]%s%s\n", color,
                 level_str(top.level).c_str(), top.time.c_str(), top.thread_id,
                 top.file_name.c_str(), top.line, top.func.c_str(),
                 top.msg.c_str(), COLOR_RESET);
        }
      }
    }
    if (Logger::get_instance()->log_file_out) {
      char temp[1024 * 10];
      if (Logger::get_instance()->log_simple_out) {
        snprintf(temp, sizeof(temp), "%-5s %s %16zx %s:%-d]%s\n",
                 level_str(top.level).c_str(), top.time.c_str(), top.thread_id,
                 top.file_name.c_str(), top.line, top.msg.c_str());
      } else {
        snprintf(temp, sizeof(temp), "%-5s %s %16zx %s:%-d %s()]%s\n",
                 level_str(top.level).c_str(), top.time.c_str(), top.thread_id,
                 top.file_name.c_str(), top.line, top.func.c_str(),
                 top.msg.c_str());
      }
      res.insert(res.end(), temp, temp + strlen(temp));
    }
    if (top.level == FATAL) {
      Logger::get_instance()->fatal = true;
      break;
    }
  }
  return res;
}

}  // namespace logger
#define LOG(Level) \
  logger::LogHolder(__FILE_NAME__, __LINE__, Level, __PRETTY_FUNCTION__).ss
#define LOG_IF(Level, Cond) \
  if (Cond)                 \
  logger::LogHolder(__FILE_NAME__, __LINE__, Level, __PRETTY_FUNCTION__).ss
#define LOG_EVERY_N(Level, N)                                  \
  static std::atomic_uint64_t __FILE_LINE__$__LINE__{0};       \
  __FILE_LINE__$__LINE__++;                                    \
  if (__FILE_LINE__$__LINE__ >= N) __FILE_LINE__$__LINE__ = 0; \
  if (__FILE_LINE__$__LINE__ == 0)                             \
  logger::LogHolder(__FILE_NAME__, __LINE__, Level, __PRETTY_FUNCTION__).ss
#define LOG_ONCE(Level)                               \
  static bool __FILE_LINE__$__LINE__ONCE_FLAG{false}; \
  bool cur_flag = false;                              \
  if (!__FILE_LINE__$__LINE__ONCE_FLAG) {             \
    __FILE_LINE__$__LINE__ONCE_FLAG = true;           \
    cur_flag = true;                                  \
  }                                                   \
  if (cur_flag)                                       \
  logger::LogHolder(__FILE_NAME__, __LINE__, Level, __PRETTY_FUNCTION__).ss
#define LOG_INFO(fmt, args...)                                               \
  {                                                                          \
    char data[128];                                                          \
    snprintf(data, sizeof(data), fmt, ##args);                               \
    logger::LogHolder(__FILE_NAME__, __LINE__, INFO, __PRETTY_FUNCTION__).ss \
        << data;                                                             \
  }
#define LOG_WARN(fmt, args...)                                               \
  {                                                                          \
    char data[128];                                                          \
    snprintf(data, sizeof(data), fmt, ##args);                               \
    logger::LogHolder(__FILE_NAME__, __LINE__, WARN, __PRETTY_FUNCTION__).ss \
        << data;                                                             \
  }
#define LOG_ERROR(fmt, args...)                                               \
  {                                                                           \
    char data[128];                                                           \
    snprintf(data, sizeof(data), fmt, ##args);                                \
    logger::LogHolder(__FILE_NAME__, __LINE__, ERROR, __PRETTY_FUNCTION__).ss \
        << data;                                                              \
  }
#define LOG_FATAL(fmt, args...)                                               \
  {                                                                           \
    char data[128];                                                           \
    snprintf(data, sizeof(data), fmt, ##args);                                \
    logger::LogHolder(__FILE_NAME__, __LINE__, FATAL, __PRETTY_FUNCTION__).ss \
        << data;                                                              \
  }
#define LOG_DEBUG(fmt, args...)                                               \
  {                                                                           \
    char data[128];                                                           \
    snprintf(data, sizeof(data), fmt, ##args);                                \
    logger::LogHolder(__FILE_NAME__, __LINE__, DEBUG, __PRETTY_FUNCTION__).ss \
        << data;                                                              \
  }
#endif