#pragma once

#include <iostream>
#include <sstream>
#include <fstream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <atomic>

#ifndef LOG_LEVEL
#define LOG_LEVEL 4
#endif

/*
0 ERROR
1 WARN
2 INFO
3 DEBUG
4 VERBOSE
*/
namespace logger{
    enum class LogLevel
        {
            ERROR = 0,
            WARN,
            INFO,
            DEBUG,
            VERBOSE
        };


inline const char* LevelToString(LogLevel lvl)
{
    switch(lvl)
    {
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::WARN: return "WARN";
        case LogLevel::INFO: return "INFO";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::VERBOSE: return "VERBOSE";
    }
    return "UNK";
}

inline const char* LevelColor(LogLevel lvl)
{
    switch(lvl)
    {
        case LogLevel::ERROR: return "\033[31m";
        case LogLevel::WARN: return "\033[33m";
        case LogLevel::INFO: return "\033[32m";
        case LogLevel::DEBUG: return "\033[36m";
        case LogLevel::VERBOSE: return "\033[37m";
    }
    return "\033[0m";
}

inline std::string CurrentTime()
{
    using namespace std::chrono;

    auto now = system_clock::now();
    auto sec = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm{};
    localtime_r(&sec, &tm);

    std::ostringstream oss;

    oss << std::put_time(&tm,"%Y-%m-%d %H:%M:%S")
        << "."
        << std::setw(3)
        << std::setfill('0')
        << ms.count();

    return oss.str();
}

struct LogMessage
{
    LogLevel level;
    std::string text;
};

class AsyncLogger
{
private:

    std::queue<LogMessage> queue;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> running{true};

    std::ofstream file;

    size_t maxFileSize = 5 * 1024 * 1024;
    std::string filename = "app.log";

public:

    AsyncLogger()
    {
        file.open(filename, std::ios::app);

        worker = std::thread([this]()
        {
            workerLoop();
        });
    }

    ~AsyncLogger()
    {
        running = false;
        cv.notify_all();
        worker.join();

        if(file.is_open())
            file.close();
    }

    void push(LogLevel lvl,const std::string& msg)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push({lvl,msg});
        }

        cv.notify_one();
    }

private:

    void rotateIfNeeded()
    {
        if(!file.is_open()) return;

        if(file.tellp() < (std::streampos)maxFileSize)
            return;

        file.close();

        std::string backup = filename + ".1";
        rename(filename.c_str(), backup.c_str());

        file.open(filename,std::ios::trunc);
    }

    void workerLoop()
    {
        while(running)
        {
            std::unique_lock<std::mutex> lock(mutex);

            cv.wait(lock,[this]
            {
                return !queue.empty() || !running;
            });

            while(!queue.empty())
            {
                auto msg = queue.front();
                queue.pop();

                lock.unlock();

                std::string line = msg.text;

                std::cout
                    << LevelColor(msg.level)
                    << line
                    << "\033[0m"
                    << std::endl;

                if(file.is_open())
                {
                    rotateIfNeeded();
                    file << line << std::endl;
                }

                lock.lock();
            }
        }
    }
};

class LogStream;

class Logger
{
private:

    std::string appId;
    std::string ctxId;

    static AsyncLogger& backend()
    {
        static AsyncLogger logger;
        return logger;
    }

public:

    Logger(const std::string& app="APP",
           const std::string& ctx="MAIN")
        : appId(app), ctxId(ctx)
    {}

    LogStream Log(LogLevel lvl);

    LogStream LogInfo();
    LogStream LogWarn();
    LogStream LogError();
    LogStream LogDebug();
    LogStream LogVerbose();

    void commit(LogLevel lvl,const std::string& text)
    {
        backend().push(lvl,text);
    }

    const std::string& App() const { return appId; }
    const std::string& Ctx() const { return ctxId; }
};

class LogStream
{
private:

    std::ostringstream buffer;
    Logger& logger;
    LogLevel level;

public:

    LogStream(Logger& lg,LogLevel lvl)
        : logger(lg), level(lvl)
    {}

    ~LogStream()
    {
        std::ostringstream line;

        line
        << "["
        << CurrentTime()
        << "]"
        << "["
        << LevelToString(level)
        << "]"
        << "["
        << logger.App()
        << "]"
        << "["
        << logger.Ctx()
        << "]"
        << "["
        << buffer.str()
        << "]";

        logger.commit(level,line.str());
    }

    template<typename T>
    LogStream& operator<<(const T& v)
    {
        buffer<<v;
        return *this;
    }
};

inline LogStream Logger::Log(LogLevel lvl)
{
    return LogStream(*this,lvl);
}

inline LogStream Logger::LogInfo()
{
#if LOG_LEVEL >= 2
    return Log(LogLevel::INFO);
#else
    return Log(LogLevel::ERROR);
#endif
}

inline LogStream Logger::LogWarn()
{
#if LOG_LEVEL >= 1
    return Log(LogLevel::WARN);
#else
    return Log(LogLevel::ERROR);
#endif
}

inline LogStream Logger::LogError()
{
    return Log(LogLevel::ERROR);
}

inline LogStream Logger::LogDebug()
{
#if LOG_LEVEL >= 3
    return Log(LogLevel::DEBUG);
#else
    return Log(LogLevel::ERROR);
#endif
}

inline LogStream Logger::LogVerbose()
{
#if LOG_LEVEL >= 4
    return Log(LogLevel::VERBOSE);
#else
    return Log(LogLevel::ERROR);
#endif
}
}