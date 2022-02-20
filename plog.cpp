#include "plog.h"
#include <atomic>

class LogThread {
public:
    LogThread()
    {
        cnt_.store(0);
        exit_.store(false);
        auto xth = ([&] {
            cnt_++;
            while (true) {
                if (exit_) {
                    break;
                }
                std::unique_lock<std::mutex> lck(Logger::mu);
                // 如果标志位不为true，则等待
                Logger::cv.wait(lck, [&] { return Logger::ready; });
                auto* temp = new LogBuffer(Logger::len);
                if (!Logger::curr_in_buffer->empty()) {
                    Logger::curr_in_buffer->setStatus(LogBuffer::status::FULL);
                    Logger::curr_in_buffer = std::shared_ptr<LogBuffer>(temp);
                    Logger::log_date.push_front(Logger::curr_in_buffer);
                }
                Logger::file->file.open(Logger::path + "/" + Logger::file->curr_file_name,
                    std::ios::binary | std::ios::app | std::ios::in | std::ios::out);
                while (Logger::curr_in_buffer != Logger::curr_out_buffer) {
                    if (Logger::curr_out_buffer->getStatus() == LogBuffer::status::FULL) {
                        std::string msg = (Logger::log_date.back())->getData();
                        Logger::write(msg);
                    }
                    Logger::log_date.pop_back();
                    Logger::curr_out_buffer = Logger::log_date.back();
                }
                Logger::file->file.close();
                Logger::ready = false;
                lck.unlock();
            }
            cnt_--;
        });
        th = std::make_shared<std::thread>(xth);
        auto x_ = ([&] {
            cnt_++;
            while (true) {
                if (exit_) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::unique_lock<std::mutex> lck(Logger::mu);
                Logger::ready = true;
                Logger::cv.notify_all();
                lck.unlock();
            }
            cnt_--;
        });
        th_ = std::make_shared<std::thread>(x_);
        th_->detach();
        th->detach();
    }
    ~LogThread()
    {
        std::this_thread::sleep_for(std::chrono::seconds(4));
        exit_.store(true);
        while (cnt_) { }
    }

private:
    std::shared_ptr<std::thread> th;
    std::shared_ptr<std::thread> th_;
    std::atomic<bool> exit_;            //是否退出
    std::atomic<unsigned short> cnt_;   //线程计数
};

Logger::Logger()
    = default;

void Logger::write(const std::string& msg)
{
    file->writeMessage(msg);
}

void Logger::addToBuffer(const std::string& msg)
{
    std::unique_lock<std::mutex> mut(mu);
    if ((*log_date.begin())->append(msg)) {
    } else {
        auto* temp = new LogBuffer(len);
        curr_in_buffer = std::shared_ptr<LogBuffer>(temp);
        log_date.push_front(curr_in_buffer);
        (*log_date.begin())->append(msg);
        ready = true;
        cv.notify_all();
    }
}

void Logger::initLogger(const std::string& argv, uint64_t len_, const std::string& path_,
    uintmax_t size_)
{
    len = len_;
    path = path_;
    size = size_;
    ready = false;
    LogFile* x = new LogFile(argv, path, size);
    file = std::shared_ptr<LogFile>(x);
    LogBuffer* temp = new LogBuffer(len);
    curr_in_buffer = std::shared_ptr<LogBuffer>(temp);
    log_date.push_back(curr_in_buffer);
    curr_out_buffer = log_date.back();
    lt = new LogThread();
}

void Logger::showDownLogger()
{
    std::this_thread::sleep_for(std::chrono::seconds(2));
    delete lt;
}

/******************************************/
LogBuffer::LogBuffer(uint64_t len)
    : max_size(len)
    , curr_pos(0)
    , can_use(len)
    , curr_status(status::FREE)
{
}

bool LogBuffer::append(const std::string& str)
{
    if (str.length() > can_use) {
        setStatus(status::FULL);
        can_use = 0;
        curr_pos = max_size;
        return false;
    } else {
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
LogFile::LogFile(const std::string& exe_name, const std::string& path, const uintmax_t size)
    : exe_name(exe_name)
    , path(path)
    , max_size(size)
{
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }
    fs::path p(exe_name);
    curr_file_name = LogTime::now().date() + "(" + std::to_string(N) + ")." + p.filename().string() + ".log";
}

std::uintmax_t LogFile::getFileSize(const std::string& file_name)
{
    return fs::file_size(file_name);
}

void LogFile::writeMessage(const std::string& msg)
{
    if (fs::exists(path + "/" + curr_file_name)) {
        if (getFileSize(path + "/" + curr_file_name) >= max_size) {
            if (file.is_open()) {
                file.close();
            }
            N++;
        }
        fs::path p(exe_name);
        curr_file_name = LogTime::now().date() + "(" + std::to_string(N) + ")." + p.filename().string() + ".log";
    }
    if (!file.is_open()) {
        file.open(path + "/" + curr_file_name, std::ios::binary | std::ios::app | std::ios::in | std::ios::out);
    }
    file << msg;
    //file.close();
}

LogFile::~LogFile()
{
    if (file.is_open()) {
        file.close();
    }
}

/**************************************************************/
LogTime LogTime::now()
{
    uint64_t timestamp;
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch())
                    .count();
    return LogTime(timestamp);
}

std::string LogTime::dateTime() const
{
    static thread_local time_t sec = 0;
    static thread_local char datetime[26] {}; //2021-01-12 12.31.24
    time_t now_sec = timestamp_ / SEC;
    if (now_sec > sec) {
        sec = now_sec;
        struct tm time_ {
        };
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
std::list<std::shared_ptr<LogBuffer>> Logger::log_date;
std::shared_ptr<LogBuffer> Logger::curr_in_buffer = nullptr;
std::shared_ptr<LogBuffer> Logger::curr_out_buffer = nullptr;
std::shared_ptr<LogFile> Logger::file = nullptr;
uint64_t Logger::len;
std::string Logger::path;
uintmax_t Logger::size;
std::mutex Logger::mu;
std::condition_variable Logger::cv;
bool Logger::ready = false;
LogThread* Logger::lt = nullptr;