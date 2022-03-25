#ifndef DAWNSEEKER_LOGGER_H
#define DAWNSEEKER_LOGGER_H

#include "global.h"
#include <chrono> 
#include <compare>
#include <iostream>
#include <memory>
#include <set>
#include <source_location>
#include <string>
#include <syncstream>

namespace logger {
    enum class LogLevel { Debug, Info, Warning, Error, Critical };

    // From lowest (Debug) to highest (Critical)
    inline auto operator <=> (LogLevel A, LogLevel B) {
        return static_cast<int>(A) <=> static_cast<int>(B);
    }

    constexpr const char* toString(LogLevel level) {
        using enum LogLevel;

        switch (level) {
            case Debug:		return "debug";
            case Info:		return "info";
            case Warning:	return "WARNING";
            case Error:		return "ERROR";
            case Critical:	return "CRITICAL";
            default:		return "(ERROR)";
        }
    }

    namespace {
        auto now() {
            return ch::time_point_cast<ch::seconds>(ch::system_clock::now());
        }
    }

    struct LogHeadFormatter {
        // Four specifiers supported:
        //	%time:		Time point of log message
        //	%_id:		Identifier of the logger
        //	%location:	Location where the log function is called
        //	%level:		Level of log message (from "debug" to "Critical")
        // e.g. "[%time] [%_id] %location %level:":
        std::string overall;
        // %time is transformed to the following time formatter
        // e.g. "%m-%d %T" (see strftime or std::formatter<std::chrono::sys_time>)
        std::string time;
        // %location is transformed to the following formatter
        // Three specifiers supported:
        //	%file:		File of log location
        //	%line:		Line number of log location
        //	%func:		Function in which the log function is called
        // e.g. "%file:%line in function `%func`"
        std::string location;

        LogHeadFormatter() noexcept = default;

        LogHeadFormatter(std::string _overall, std::string _time, std::string _location) noexcept:
        overall(std::move(_overall)), time(std::move(_time)), location(std::move(_location)) {}

        LogHeadFormatter(const char* _overall, const char* _time, const char* _location) noexcept:
        overall(_overall), time(_time), location(_location) {}

        // Transforms to the formatter string for std::format (or fmt::format)
        // {0}: %time
        // {1}: %level
        // {2}: %_id
        // {3}: %file
        // {4}: %line
        // {5}: %func
        [[nodiscard]] std::string transform() const {
            auto perform = [](std::string& dest,
                              const std::string& specifier,
                              const std::string& replacedTo) {
                auto pos = dest.find(specifier);
                if (pos != std::string::npos) {
                    dest.replace(pos, specifier.length(), replacedTo);
                }
            };

            auto res = overall;
            //perform(res, "%time", FORMAT("{{0:{0}}}", time));
            perform(res, "%time", "{0}");
            perform(res, "%level", "{1}");
            perform(res, "%_id", "{2}");

            auto loc = location;
            perform(loc, "%file", "{3}");
            perform(loc, "%line", "{4}");
            perform(loc, "%func", "{5}");

            perform(res, "%location", loc);
            return res;
        }
    };

    class Logger {
    public:
#ifdef __linux__
    // Default logger for Linux with GCC:
    // Source location is displayed as file name
    //  since in Linux the file name is shorter (parent directories is hidden)
    static inline LogHeadFormatter defaultFormatter = LogHeadFormatter{
        "[%time][%location] %level: ",
        "%m-%d %T",
        "%file:%line"
    };
#else
    // Default logger for Windows with MSVC:
    // Source location is displayed as function name
    //  since in Windows the function name is shorter
    //  (return type, arguments, etc. are hidden)
    static inline LogHeadFormatter defaultFormatter = LogHeadFormatter(
            "[%time][%location] %level: ",
            "%m-%d %T",
            "%func:%line"
    );
#endif

    private:
        // Identifier of the logger
        std::string         _id;
        // Output destination
        std::ostream&       out;
        // Formatter for std::format (or fmt::format)
        LogHeadFormatter    formatter;
        std::string         formatterString;
        // Minimum log level
        // Message lower than this level is ignored
        LogLevel		    minLevel;

    public:
        Logger(std::string id,
               std::ostream& stream,
               LogLevel minLevel,
               const LogHeadFormatter& formatter = defaultFormatter) :
                _id(std::move(id)), out(stream),
                formatter(formatter), formatterString(formatter.transform()),
                minLevel(minLevel) {}

        void setMinLevel(LogLevel level) {
            minLevel = level;
        }

        void setFormatter(LogHeadFormatter headFormatter) {
            this->formatter = std::move(headFormatter);
            this->formatterString = this->formatter.transform();
        }

        void log(LogLevel level,
                 const std::string& content,
                 const std::source_location loc = std::source_location::current()) const {
            if (level < minLevel) {
                return;
            }
            // C-style formatter is still required for fmt::v7
            static char buffer[256];
            auto timeValue = ch::system_clock::to_time_t(now());
#ifdef _WIN32
            std::tm tm{};
            localtime_s(&tm, &timeValue);
            std::strftime(buffer, sizeof(buffer), formatter.time.c_str(), &tm);
#else
            std::strftime(
                buffer, sizeof(buffer), formatter.time.c_str(), std::localtime(&timeValue)
            );
#endif
            std::osyncstream(out)
                    << format(formatterString, buffer, toString(level), _id,
                              loc.file_name(), loc.line(), loc.function_name())
                    << content << std::endl;
        }

#define FUNC(fnName, level) \
        void fnName (const std::string& content, \
                const std::source_location loc = std::source_location::current()) const { \
            log(LogLevel:: level, content, loc); \
        }

        FUNC(debug, Debug)
        FUNC(info, Info)
        FUNC(warning, Warning)
        FUNC(error, Error)
        FUNC(critical, Critical)
#undef FUNC

        // Access to _id
        [[nodiscard]] const std::string& id() const {
            return _id;
        }

        // Compares by _id
        auto operator <=> (const Logger& other) const {
            return this->_id <=> other._id;
        }
        auto operator <=> (const std::string& id) const {
            return this->_id <=> id;
        }
    };

    class Loggers {
        static inline std::vector<std::shared_ptr<Logger>> loggers;

        static auto findLogger(const std::string& id) {
            auto it = loggers.begin();
            for (; it != loggers.end(); ++it) {
                if ((**it).id() == id) {
                    break;
                }
            }
            return it;
        }

    public:
        // Adds a logger via a shared_ptr
        // Fails if another logger with the same _id is already added
        // Returns: whether the logger is added successfully
        static bool add(std::shared_ptr<Logger> logger) {
            auto it = findLogger(logger->id());
            if (it != loggers.end()) {
                return false;
            }
            loggers.push_back(std::move(logger));
            return true;
        }

        // Removes the logger with specified _id
        // Fails if the logger does not exist
        // Returns: whether the logger is removed successfully
        static bool remove(const std::string& id) {
            auto it = findLogger(id);
            if (it == loggers.end()) {
                return false;
            }
            loggers.erase(it);
            return true;
        }

        // Output to all loggers
        static void log(LogLevel level, const std::string& content,
                        const std::source_location loc = std::source_location::current()) {
            for (const auto& logger : loggers) {
                logger->log(level, content, loc);
            }
        }

        // Output to the specified logger
        // Warning message is output to std::cerr if no match to _id
        static void log(LogLevel level, const std::string& id, const std::string& content,
                        const std::source_location loc = std::source_location::current()) {
            if (auto it = findLogger(id); it != loggers.end()) {
                (**it).log(level, content, loc);
            }
            else {
                std::cerr << format(
                        "WARNING on Logger::log: Logger with _id '{}' is not found.", id
                        ) << std::endl;
            }
        }

#define FUNC(fnName, logLevel) \
        static void fnName (const std::string& content, \
            const std::source_location loc = std::source_location::current()) { \
            log(LogLevel:: logLevel, content, loc); \
        } \
        static void fnName (const std::string& id, const std::string& content, \
            const std::source_location loc = std::source_location::current()) { \
            log(LogLevel:: logLevel, id, content, loc); \
        }

        FUNC(debug, Debug)
        FUNC(info, Info)
        FUNC(warning, Warning)
        FUNC(error, Error)
        FUNC(critical, Critical)
#undef FUNC
    };
}

#if !defined(LOG_LEVEL) || LOG_LEVEL <= 0
#define LOG_DEBUG(...) logger::Loggers::debug(__VA_ARGS__)
#endif

#if !defined(LOG_LEVEL) || LOG_LEVEL <= 1
#define LOG_INFO(...) logger::Loggers::info(__VA_ARGS__)
#endif

#if !defined(LOG_LEVEL) || LOG_LEVEL <= 2
#define LOG_WARNING(...) logger::Loggers::warning(__VA_ARGS__)
#endif

#if !defined(LOG_LEVEL) || LOG_LEVEL <= 3
#define LOG_ERROR(...) logger::Loggers::error(__VA_ARGS__)
#define LOG_CRITICAL(...) logger::Loggers::critical(__VA_ARGS__)
#endif

#endif //DAWNSEEKER_LOGGER_H