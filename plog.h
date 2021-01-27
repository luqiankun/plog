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

    status getStatus() const
    { return curr_status; };

    std::string getData()
    { return data; };

    bool empty() const
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
    static std::shared_ptr<std::thread> th;
    static std::shared_ptr<std::thread> th_;
    static bool ready;
};

/***********************************************/
Logger::Logger()
= default;

void Logger::write(const std::string &msg)
{
    file->writeMessage(msg);
}

void Logger::addToBuffer(const std::string &msg)
{
    std::unique_lock<std::mutex> mut(mu);
    if ((*log_date.begin())->append(msg))
    {
    } else
    {
        auto *temp = new LogBuffer(len);
        curr_in_buffer = std::shared_ptr<LogBuffer>(temp);
        log_date.push_front(curr_in_buffer);
        (*log_date.begin())->append(msg);
        ready = true;
        cv.notify_all();
    }
}

void Logger::initLogger(const std::string &argv, uint64_t len_, const std::string &path_,
                        uintmax_t size_)
{
    len = len_;
    path = path_;
    size = size_;
    ready = false;
    auto x = new LogFile(argv, path, size);
    file = std::shared_ptr<LogFile>(x);
    auto *temp = new LogBuffer(len);
    curr_in_buffer = std::shared_ptr<LogBuffer>(temp);
    log_date.push_back(curr_in_buffer);
    curr_out_buffer = log_date.back();
    auto xth = ([&]
    {
        while (true)
        {
            std::unique_lock<std::mutex> lck(mu);
            // 如果标志位不为true，则等待
            cv.wait(lck, [&]
            { return ready; });
            auto *temp = new LogBuffer(len);
            if (!curr_in_buffer->empty())
            {
                curr_in_buffer->setStatus(LogBuffer::status::FULL);
                curr_in_buffer = std::shared_ptr<LogBuffer>(temp);
                log_date.push_front(curr_in_buffer);
            }
            file->file.open(path + "/" + file->curr_file_name,
                            std::ios::binary | std::ios::app | std::ios::in | std::ios::out);
            while (curr_in_buffer != curr_out_buffer)
            {
                if (curr_out_buffer->getStatus() == LogBuffer::status::FULL)
                {
                    std::string msg = (log_date.back())->getData();
                    write(msg);
                }
                log_date.pop_back();
                curr_out_buffer = log_date.back();
            }
            file->file.close();
            ready = false;
            lck.unlock();
        }
    });
    th = std::make_shared<std::thread>(xth);
    auto x_ = ([&]
    {
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::unique_lock<std::mutex> lck(mu);
            ready = true;
            cv.notify_all();
            lck.unlock();
        }
    });
    th_ = std::make_shared<std::thread>(x_);
    th_->detach();
    th->detach();
}

void Logger::showDownLogger()
{std::this_thread::sleep_for(std::chrono::seconds(2));}

/******************************************/
LogBuffer::LogBuffer(uint64_t len) : max_size(len), curr_pos(0), can_use(len), curr_status(status::FREE)
{}

bool LogBuffer::append(const std::string &str)
{
    if (str.length() > can_use)
    {
        setStatus(status::FULL);
        can_use = 0;
        curr_pos = max_size;
        return false;
    } else
    {
        data.append(str);
        curr_pos += str.length();
        can_use -= str.length();
        return true;
    }
}

void LogBuffer::setStatus(status sta)
{
    curr_status = sta;
}

/*************************************************/
LogFile::LogFile(const std::string &exe_name, const std::string &path, const uintmax_t size) : exe_name(exe_name),
                                                                                               path(path),
                                                                                               max_size(size)
{
    if (!fs::exists(path))
    {
        fs::create_directories(path);
    }
    fs::path p(exe_name);
    curr_file_name = LogTime::now().date() + "(" + std::to_string(N) + ")." + p.filename().string() + ".log";
}

std::uintmax_t LogFile::getFileSize(const std::string &file_name)
{
    return fs::file_size(file_name);
}

void LogFile::writeMessage(const std::string &msg)
{
    if (fs::exists(path + "/" + curr_file_name))
    {
        if (getFileSize(path + "/" + curr_file_name) >= max_size)
        {
            if (file.is_open())
            {
                file.close();
            }
            N++;
        }
        fs::path p(exe_name);
        curr_file_name = LogTime::now().date() + "(" + std::to_string(N) + ")." + p.filename().string() + ".log";
    }
    if (!file.is_open())
    {
        file.open(path + "/" + curr_file_name, std::ios::binary | std::ios::app | std::ios::in | std::ios::out);
    }
    file << msg;
    //file.close();
}

LogFile::~LogFile()
{
    if (file.is_open())
    {
        file.close();
    }
}

/**************************************************************/
LogTime LogTime::now()
{
    uint64_t timestamp;
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    return LogTime(timestamp);
}

std::string LogTime::dateTime() const
{
    static thread_local time_t sec = 0;
    static thread_local char datetime[26]{};//2021-01-12 12.31.24
    time_t now_sec = timestamp_ / SEC;
    if (now_sec > sec)
    {
        sec = now_sec;
        struct tm time_{};
#ifdef __linux
        localtime_r(&sec, &time_);
#else
        localtime_s(&time_, &sec);
#endif // __linux

        strftime(datetime, sizeof(datetime), "%Y-%m-%d %H.%M.%S", &time_);
    }
    return datetime;
}

std::string LogTime::formatTime() const
{
    char format[28];
    auto micro = static_cast<uint32_t>(timestamp_ % SEC);
    snprintf(format, sizeof(format), "%s.%06u", dateTime().c_str(), micro);
    return format;
}

/*******************************/
std::list<std::shared_ptr<LogBuffer>>  Logger::log_date;
std::shared_ptr<LogBuffer> Logger::curr_in_buffer = nullptr;
std::shared_ptr<LogBuffer> Logger::curr_out_buffer = nullptr;
std::shared_ptr<LogFile> Logger::file = nullptr;
uint64_t Logger::len;
std::string Logger::path;
uintmax_t Logger::size;
std::mutex Logger::mu;
std::condition_variable Logger::cv;
std::shared_ptr<std::thread> Logger::th;
std::shared_ptr<std::thread> Logger::th_;
bool Logger::ready = false;

#endif //UNTITLED_PLOG_H
