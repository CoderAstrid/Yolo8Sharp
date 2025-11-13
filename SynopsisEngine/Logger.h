#pragma once

// Prevent Windows.h from defining min/max macros that conflict with std::min/std::max
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <sstream>
#include <mutex>
#include <fstream>
#include <memory>
#include <iomanip>
#include <iostream>
#include <Windows.h>
#include <algorithm>  // For std::min, std::max

// Log levels
enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4
};

// Logger class - thread-safe, works in both DLL and EXE
class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    // Set minimum log level (messages below this level are ignored)
    void SetLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        minLevel_ = level;
    }

    // Enable/disable file logging
    void SetFileLogging(bool enable, const std::string& filename = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        fileLoggingEnabled_ = enable;
        if (enable && !filename.empty()) {
            logFile_.open(filename, std::ios::app);
        } else if (!enable && logFile_.is_open()) {
            logFile_.close();
        }
    }

    // Enable/disable stdout redirection (redirects stdout/stderr to OutputDebugString)
    void SetStdoutRedirection(bool enable) {
        std::lock_guard<std::mutex> lock(mutex_);
        stdoutRedirectionEnabled_ = enable;
    }

    // Log a message
    void Log(LogLevel level, const std::string& message, const char* file = nullptr, int line = 0) {
        if (level < minLevel_) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        
        // Build log message
        std::ostringstream oss;
        
        // Add timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        oss << "[" << std::setfill('0') << std::setw(2) << st.wHour << ":"
            << std::setw(2) << st.wMinute << ":" << std::setw(2) << st.wSecond << "."
            << std::setw(3) << st.wMilliseconds << "] ";
        
        // Add log level
        const char* levelStr = GetLevelString(level);
        oss << "[" << levelStr << "] ";
        
        // Add file and line if provided
        if (file && line > 0) {
            // Extract just the filename from the path
            const char* filename = strrchr(file, '\\');
            if (!filename) filename = strrchr(file, '/');
            if (filename) filename++;
            else filename = file;
            oss << "[" << filename << ":" << line << "] ";
        }
        
        // Add message
        oss << message;
        
        // Ensure newline
        std::string fullMessage = oss.str();
        if (!fullMessage.empty() && fullMessage.back() != '\n') {
            fullMessage += "\n";
        }
        
        // Output to debug window (works in both DLL and EXE when debugger is attached)
        OutputDebugStringA(fullMessage.c_str());
        
        // Output to file if enabled
        if (fileLoggingEnabled_ && logFile_.is_open()) {
            logFile_ << fullMessage;
            logFile_.flush();
        }
        
        // Output to stdout if redirection is enabled (for console apps)
        if (stdoutRedirectionEnabled_) {
            std::cout << fullMessage;
            std::cout.flush();
        }
    }

    // Convenience methods
    void Debug(const std::string& message, const char* file = nullptr, int line = 0) {
        Log(LogLevel::Debug, message, file, line);
    }

    void Info(const std::string& message, const char* file = nullptr, int line = 0) {
        Log(LogLevel::Info, message, file, line);
    }

    void Warning(const std::string& message, const char* file = nullptr, int line = 0) {
        Log(LogLevel::Warning, message, file, line);
    }

    void Error(const std::string& message, const char* file = nullptr, int line = 0) {
        Log(LogLevel::Error, message, file, line);
    }

    void Fatal(const std::string& message, const char* file = nullptr, int line = 0) {
        Log(LogLevel::Fatal, message, file, line);
    }

private:
    Logger() : minLevel_(LogLevel::Debug), fileLoggingEnabled_(false), stdoutRedirectionEnabled_(false) {}
    ~Logger() {
        if (logFile_.is_open()) {
            logFile_.close();
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    const char* GetLevelString(LogLevel level) const {
        switch (level) {
            case LogLevel::Debug:   return "DEBUG";
            case LogLevel::Info:    return "INFO ";
            case LogLevel::Warning: return "WARN ";
            case LogLevel::Error:   return "ERROR";
            case LogLevel::Fatal:   return "FATAL";
            default:                return "UNK  ";
        }
    }

    std::mutex mutex_;
    LogLevel minLevel_;
    bool fileLoggingEnabled_;
    bool stdoutRedirectionEnabled_;
    std::ofstream logFile_;
};

// Convenience macros for logging with file and line information
#define LOG_DEBUG(msg)   Logger::Instance().Debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg)    Logger::Instance().Info(msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) Logger::Instance().Warning(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg)   Logger::Instance().Error(msg, __FILE__, __LINE__)
#define LOG_FATAL(msg)   Logger::Instance().Fatal(msg, __FILE__, __LINE__)

// Stream-based logging macros (for formatted output)
#define LOG_DEBUG_STREAM(stream)   do { std::ostringstream oss; oss << stream; Logger::Instance().Debug(oss.str(), __FILE__, __LINE__); } while(0)
#define LOG_INFO_STREAM(stream)    do { std::ostringstream oss; oss << stream; Logger::Instance().Info(oss.str(), __FILE__, __LINE__); } while(0)
#define LOG_WARNING_STREAM(stream) do { std::ostringstream oss; oss << stream; Logger::Instance().Warning(oss.str(), __FILE__, __LINE__); } while(0)
#define LOG_ERROR_STREAM(stream)   do { std::ostringstream oss; oss << stream; Logger::Instance().Error(oss.str(), __FILE__, __LINE__); } while(0)
#define LOG_FATAL_STREAM(stream)   do { std::ostringstream oss; oss << stream; Logger::Instance().Fatal(oss.str(), __FILE__, __LINE__); } while(0)

