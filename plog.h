//
// Created by luqia on 2021/1/12.
//

#ifndef UNTITLED_PLOG_H
#define UNTITLED_PLOG_H

#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <ctime>
#include <cstring>

#if (_WIN32 || WIN64)
#define MY_FILE(x) strrchr(x,'\\')?strrchr(x,'\\')+1:x
#else
#define MY_FILE(x) strrchr(x,'/')?strrchr(x,'/')+1:x
#endif

inline std::string pid()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

namespace fs = std::filesystem;
#define LOG_INFO(msg) Logger::addToBuffer("[INFO "+LogTime::now().formatTime()+" "+pid()+" "+(MY_FILE(__FILE__))+":"+std::to_string(__LINE__)+"-->"+__FUNCTION__+" ] "+msg+"\n");
#define LOG_ERROR(msg) Logger::addToBuffer("[ERROR "+LogTime::now().formatTime()+" "+pid()+" "+(MY_FILE(__FILE__))+":"+std::to_string(__LINE__)+"-->"+__FUNCTION__+" ] "+msg+"\n");
#define LOG_WARN(msg) Logger::addToBuffer("[WARN "+LogTime::now().formatTime()+" "+pid()+" "+(MY_FILE(__FILE__))+":"+std::to_string(__LINE__)+"-->"+__FUNCTION__+" ] "+msg+"\n");
#define LOG_DEBUG(msg) Logger::addToBuffer("[DEBUG "+LogTime::now().formatTime()+" "+pid()+" "+(MY_FILE(__FILE__))+":"+std::to_string(__LINE__)+"-->"+__FUNCTION__+" ] "+msg+"\n");

//用于管理后台线程
class LogThread;

//获取时间并格式化，提供两种时间格式，一种精确到毫秒（6位小数点），一种精确到小时，作为日志文件的名称使用(每隔1小时一个log文件)
class LogTime
{
public :
    LogTime() : timestamp_(0)
    {}

    explicit LogTime(uint64_t timestamp) : timestamp_(timestamp)
    {}

    static LogTime now();

    std::string date() const
    { return std::string(dateTime(), 0, 13); };

    std::string dateTime() const;

    std::string formatTime() const;

private:
    uint64_t timestamp_;
    static const uint32_t SEC = 1000000;
};

//日志基础容器类，包装了一个std::string，提供状态、可用空间、加入日志、清空、是否为空、返回数据等操作
class LogBuffer
{
public:
    enum status
    {
        FULL = 1,
        FREE = 0
    };

    explicit LogBuffer(uint64_t len);

    bool append(const std::string &str);

    void setStatus(status sta);

    inline status getStatus() const
    { return curr_status; };

    inline std::string getData()
    { return data; };

    inline bool empty() const
    { return can_use == max_size; }

private:
    status curr_status;
    uint64_t curr_pos;
    int64_t can_use;
    std::string data;
    uint64_t max_size;
};


class LogFile
{

public:
    //exe_name: 生成日志名称
    LogFile(const std::string &exe_name, const std::string &path, uintmax_t size);

    static std::uintmax_t getFileSize(const std::string &file_name);

    void writeMessage(const std::string &msg);

    ~LogFile();

public:
    std::ofstream file;
    std::string curr_file_name;
    std::string path;
    uintmax_t max_size;
    std::string exe_name;
private:
    uint32_t N = 0;
};

class Logger
{
public:
    explicit Logger();

    static void write(const std::string &msg);

    static void addToBuffer(const std::string &msg);

    static void initLogger(const std::string &argv, uint64_t len = 4096, const std::string &path = "./log",
                           uintmax_t size = 1000 * 10 * 1024);//默认单个文件10MB
//TODO 日志关闭功能，目前仅仅是使用延时来等待关闭前保证数据写入
    static void showDownLogger();;
public:
    static std::list<std::shared_ptr<LogBuffer>> log_date;//log数据
    static std::shared_ptr<LogBuffer> curr_in_buffer;//当前的写入buffer
    static std::shared_ptr<LogBuffer> curr_out_buffer;//当前的持久化buffer
    static std::shared_ptr<LogFile> file;
    static uint64_t len;
    static std::string path;
    static uintmax_t size;
    static std::mutex mu;
    static std::condition_variable cv;
    static bool ready;
    static LogThread* lt;
};

#endif //UNTITLED_PLOG_H
