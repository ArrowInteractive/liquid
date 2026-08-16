#include "utils/Log.hpp"
#include <iostream>
#include <mutex>

namespace {
std::mutex g_log_mutex;
}

LogStream::~LogStream()
{
    const char *tag;
    std::ostream *out;
    switch (level) {
        case LogLevel::Warn:  tag = "[WARN] ";  out = &std::cerr; break;
        case LogLevel::Error: tag = "[ERROR] "; out = &std::cerr; break;
        default:              tag = "[INFO] ";  out = &std::cout; break;
    }

    std::lock_guard<std::mutex> lock(g_log_mutex);
    *out << tag << buffer.str() << std::endl;
}

LogStream Log::info()  { return LogStream(LogLevel::Info); }
LogStream Log::warn()  { return LogStream(LogLevel::Warn); }
LogStream Log::error() { return LogStream(LogLevel::Error); }
