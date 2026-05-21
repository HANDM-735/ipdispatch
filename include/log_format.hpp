#ifndef LOG_FORMAT_HPP
#define LOG_FORMAT_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <memory>
#include <atomic>
#include <cstdarg>

#define TIME_BUFF_LEN          (80)
#define MAX_MSG_LEN            (1 * 1024)
#define LOG_MSG(level, fmt, ...) \
do { \
    Logger::getInstance().log(level, fmt, ##__VA_ARGS__); \
} while(0)

// 日志级别
enum Level {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    // 获取单例实例
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // 初始化日志系统
    bool init(const std::string& filename = "", Level level = INFO) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return false; // 已经初始化
        }

        level_ = level;

        if (!filename.empty()) {
            std::string temp_name = filename + "-" + get_current_time(2) + ".log";
            file_.open(temp_name, std::ios::out | std::ios::app);
            if (!file_.is_open()) {
                std::cerr << "Can't open file: " << temp_name << std::endl;
                return false;
            }
            useFile_ = true;
        }

        running_ = true;
        workerThread_ = std::thread(&Logger::processEntries, this);
        return true;
    }

    // 关闭日志系统
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
        condVar_.notify_all();
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        if (file_.is_open()) {
            file_.close();
        }
    }

    // 记录日志（带格式化）
    void log(Level level, const char* format, ...) {
        if (level < level_) {
            return; // 低于当前日志级别，不记录
        }

        if (!running_) {
            return; // 记录日志线程未启动，不记录
        }

        // 格式化消息
        va_list args;
        va_start(args, format);
        char buffer[MAX_MSG_LEN];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream stream;
        stream << get_current_time(1)
               << "." << std::setfill('0') << std::setw(3) << now_ms.count()
               << " [" << levelToString(level) << "] "
               << buffer << std::endl;

        std::string logEntry = stream.str();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(logEntry);
        }
        condVar_.notify_one();
    }

    // 便捷方法（带格式化）
    void debug(const char* format, ...) {
        if (DEBUG < level_) return;

        va_list args;
        va_start(args, format);
        char buffer[MAX_MSG_LEN];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        log(DEBUG, buffer);
    }

    void info(const char* format, ...) {
        if (INFO < level_) return;

        va_list args;
        va_start(args, format);
        char buffer[MAX_MSG_LEN];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        log(INFO, buffer);
    }

    void warning(const char* format, ...) {
        if (WARNING < level_) return;

        va_list args;
        va_start(args, format);
        char buffer[MAX_MSG_LEN];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        log(WARNING, buffer);
    }

    void error(const char* format, ...) {
        if (ERROR < level_) return;

        va_list args;
        va_start(args, format);
        char buffer[MAX_MSG_LEN];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        log(ERROR, buffer);
    }

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() : running_(false), useFile_(false), level_(INFO) {}
    ~Logger() {
        shutdown();
    }

    // 日志级别转字符串
    std::string levelToString(Level level) {
        switch (level) {
            case DEBUG: return "DEBUG";
            case INFO: return "INFO";
            case WARNING: return "WARNING";
            case ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    // 工作线程函数
    void processEntries() {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex_);

            // 等待直到有日志条目或需要关闭
            condVar_.wait(lock, [this]() {
                return !queue_.empty() || !running_;
            });

            // 处理所有当前队列中的日志条目
            while (!queue_.empty()) {
                std::string entry = queue_.front();
                queue_.pop();

                // 释放锁，以便其他线程可以继续添加日志
                lock.unlock();

                // 输出到控制台
                std::cout << entry;

                // 输出到文件（如果启用）
                if (useFile_ && file_.is_open()) {
                    file_ << entry;
                    file_.flush(); // 确保立即写入
                }

                // 重新获取锁以便检查队列
                lock.lock();
            }

            if (!running_) {
                break;
            }
        }
    }

    std::string get_current_time(int mode) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&in_time_t, &tm); // 使用线程安全的版本

        char buffer[TIME_BUFF_LEN];
        if (mode == 1) {
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        } else {
            strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H-%M-%S", &tm);
        }
        return buffer;
    }

private:
    std::queue<std::string> queue_;
    std::mutex mutex_;
    std::condition_variable condVar_;
    std::thread workerThread_;
    std::ofstream file_;
    std::atomic<bool> running_;
    bool useFile_;
    Level level_;
};

#endif /* LOG_FORMAT_HPP */