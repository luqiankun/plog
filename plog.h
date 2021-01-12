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
#include <string>
#include<chrono>
#include <ctime>

namespace fs = std::filesystem;
#define LOG_INFO(msg) {std::stringstream ss; \
                    ss<<"[INFO "<<LogTime::now().formatTime()<<" "<<std::this_thread::get_id()<<" "<<fs::path(__FILE__).filename().string()<<":"<<std::to_string(__LINE__)<<"]"<<msg<<std::endl;\
                        logger.addToBuffer(ss.str());\
}
#define LOG_WARN(msg) {std::stringstream ss; \
                    ss<<"[WARN "<<LogTime::now().formatTime()<<" "<<std::this_thread::get_id()<<" "<<fs::path(__FILE__).filename().string()<<":"<<std::to_string(__LINE__)<<"]"<<msg<<std::endl;\
                        logger.addToBuffer(ss.str());\
}
#define LOG_DEBUG(msg) {std::stringstream ss; \
                    ss<<"[DEBUG "<<LogTime::now().formatTime()<<" "<<std::this_thread::get_id()<<" "<<fs::path(__FILE__).filename().string()<<":"<<std::to_string(__LINE__)<<"]"<<msg<<std::endl;\
                        logger.addToBuffer(ss.str());\
}
#define LOG_ERROR(msg) {std::stringstream ss; \
                    ss<<"[ERROR "<<LogTime::now().formatTime()<<" "<<std::this_thread::get_id()<<" "<<fs::path(__FILE__).filename().string()<<":"<<std::to_string(__LINE__)<<"]"<<msg<<std::endl;\
                        logger.addToBuffer(ss.str());\
}

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

    std::string time() const;

    std::string formatTime() const;

    uint64_t timestamp() const
    { return timestamp_; }

private:
    uint64_t timestamp_;
    static const uint32_t KUSEC = 1000000;
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

    uint64_t getAvail() const
    { return can_use; }

    void setStatus(status sta);

    status getStatus() const
    { return curr_status; };

    void clear()
    {
        curr_pos = 0;
        curr_status = status::FREE;
        can_use = max_size;
        data = "";
    };

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
    LogFile(const std::string &path, uintmax_t size);

    static std::uintmax_t getFileSize(const std::string &file_name);

    void writeMessage(const std::string &msg);

    ~LogFile();

public:
    std::ofstream file;
    std::string curr_file_name;
    std::string path;
    uintmax_t max_size;
private:
    LogTime Time;
    uint32_t N = 0;
};

class Logger
{
public:
    explicit Logger(uint64_t len = 4096, const std::string &path = "./log", uintmax_t size = 1000 * 10 * 1024);

    void write(const std::string &msg) const;

    void addToBuffer(const std::string &msg);

    void initLogger();

public:
    std::list<std::shared_ptr<LogBuffer>> log_date;//log数据
    std::shared_ptr<LogBuffer> curr_in_buffer;//当前的写入buffer
    std::shared_ptr<LogBuffer> curr_out_buffer;//当前的持久化buffer
    std::shared_ptr<LogFile> file;
    uint64_t len;
    std::string path;
    uintmax_t size;
    std::mutex mu;
    std::condition_variable cv;
    std::shared_ptr<std::thread> th;
    std::shared_ptr<std::thread> th_;
    bool ready;
};

/***********************************************/
Logger::Logger(uint64_t len_, const std::string &path_, const uintmax_t size_)
{
    len = len_;
    path = path_;
    size = size_;
    ready = false;
    auto x = new LogFile(path, size);
    file = std::shared_ptr<LogFile>(x);
    auto *temp = new LogBuffer(len);
    curr_in_buffer = std::shared_ptr<LogBuffer>(temp);
    log_date.push_back(curr_in_buffer);
    curr_out_buffer = log_date.back();
}

void Logger::write(const std::string &msg) const
{
    file->writeMessage(msg);

}

void Logger::addToBuffer(const std::string &msg)
{
    std::unique_lock<std::mutex> mut(mu);
    if ((*log_date.begin())->append(msg))
    {
    }
    else
    {
        auto *temp = new LogBuffer(len);
        curr_in_buffer = std::shared_ptr<LogBuffer>(temp);
        log_date.push_front(curr_in_buffer);
        (*log_date.begin())->append(msg);
        ready = true;
        cv.notify_all();
    }
}

void Logger::initLogger()
{
    auto x = ([&]
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
            ready = false;
            lck.unlock();
        }
    });
    th = std::make_shared<std::thread>(x);
    auto x_ = ([&]
    {
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
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

/******************************************/
LogBuffer::LogBuffer(uint64_t len) : max_size(len), curr_pos(0), can_use(len), curr_status(status::FREE)
{
}

bool LogBuffer::append(const std::string &str)
{
    if (str.length() > can_use)
    {
        setStatus(status::FULL);
        can_use = 0;
        curr_pos = max_size;
        return false;
    }
    else
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
LogFile::LogFile(const std::string &path, const uintmax_t size) : path(path), max_size(size)
{
    if (!fs::exists(path))
    {
        fs::create_directories(path);
    }
    curr_file_name = LogTime::now().date() + "(" + std::to_string(N) + ").log";
}

std::uintmax_t LogFile::getFileSize(const std::string &file_name)
{
    return fs::file_size(file_name);
}

void LogFile::writeMessage(const std::string &msg)
{
    if (fs::exists(path + "\/" + curr_file_name))
    {
        if (getFileSize(path + "\/" + curr_file_name) >= max_size)
        {
            if (file.is_open())
            {
                file.close();
            }
            N++;
        }
        curr_file_name = LogTime::now().date() + "(" + std::to_string(N) + ").log";
    }
    if (!file.is_open())
    {
        file.open(path + "\/" + curr_file_name, std::ios::app);
    }
    file << msg;
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
    static thread_local char datatime[26]{};//2021-01-12 12.31.24
    time_t nowsec = timestamp_ / KUSEC;
    if (nowsec > sec)
    {
        sec = nowsec;
        struct tm time_{};
#ifdef __linux
        localtime_r(&sec, &time_);
#else
        localtime_s(&time_, &sec);
#endif // __linux

        strftime(datatime, sizeof(datatime), "%Y-%m-%d %H.%M.%S", &time_);
    }
    return datatime;
}

std::string LogTime::time() const
{
    std::string time(dateTime(), 11, 18);
    time.erase(std::remove(time.begin(), time.end(), '.'), time.end());
    return time;
}

std::string LogTime::formatTime() const
{
    char format[28];
    auto micro = static_cast<uint32_t>(timestamp_ % KUSEC);
    snprintf(format, sizeof(format), "%s.%06u", dateTime().c_str(), micro);
    return format;
}

#endif //UNTITLED_PLOG_H
