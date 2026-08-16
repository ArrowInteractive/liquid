#pragma once

#include <sstream>

enum class LogLevel { Info, Warn, Error };

// Accumulates a log line via operator<< (so old "cout << a << b" call sites
// port over almost unchanged), then writes it out as a single line when it
// goes out of scope - Info to stdout, Warn/Error to stderr, each tagged with
// its level. Writing the whole line at once (instead of chaining "<<"
// straight to cout/cerr, like the code used to) also keeps lines from
// different threads from interleaving mid-message.
class LogStream
{
    public:
        explicit LogStream(LogLevel level) : level(level) {}
        ~LogStream();

        LogStream(const LogStream &) = delete;
        LogStream &operator=(const LogStream &) = delete;

        template <typename T>
        LogStream &operator<<(const T &value)
        {
            buffer << value;
            return *this;
        }

    private:
        LogLevel level;
        std::ostringstream buffer;
};

class Log
{
    public:
        static LogStream info();
        static LogStream warn();
        static LogStream error();
};
