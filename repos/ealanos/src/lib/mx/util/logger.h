#pragma once

#include <iostream>
#include <mx/system/environment.h>
#include <ostream>
#include <string>

namespace mx::util {
class Logger
{
public:
    enum Level
    {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    static void debug([[maybe_unused]] std::string &&message) noexcept
    {
        if constexpr (system::Environment::is_debug())
        {
            log<Level::DEBUG>(std::cout, std::move(message));
        }
    }

    static void debug_if([[maybe_unused]] const bool guard, [[maybe_unused]] std::string &&message) noexcept
    {
        if constexpr (system::Environment::is_debug())
        {
            log_if<Level::DEBUG>(guard, std::cout, std::move(message));
        }
    }

    static void info(std::string &&message) noexcept { log<Level::INFO>(std::cout, std::move(message)); }

    static void info_if(const bool guard, std::string &&message) noexcept
    {
        log_if<Level::INFO>(guard, std::cout, std::move(message));
    }

    static void warn(std::string &&message) noexcept { log<Level::WARNING>(std::cerr, std::move(message)); }

    static void warn_if(const bool guard, std::string &&message) noexcept
    {
        log_if<Level::WARNING>(guard, std::cerr, std::move(message));
    }

    static void error(std::string &&message) noexcept { log<Level::ERROR>(std::cerr, std::move(message)); }

    static void error_if(const bool guard, std::string &&message) noexcept
    {
        log_if<Level::ERROR>(guard, std::cerr, std::move(message));
    }

private:
    template <Level L> static void log(std::ostream &stream, std::string &&message) noexcept
    {
        stream << "[" << to_string<L>() << "] " << message << std::endl;
    }

    template <Level L> static void log_if(const bool guard, std::ostream &stream, std::string &&message) noexcept
    {
        if (guard == true)
        {
            log<L>(stream, std::move(message));
        }
    }

    template <Level L> [[nodiscard]] static std::string to_string() noexcept
    {
        if constexpr (L == Level::DEBUG)
        {
            return "debug  ";
        }
        else if constexpr (L == Level::INFO)
        {
            return "info   ";
        }
        else if constexpr (L == Level::WARNING)
        {
            return "warning";
        }
        else
        {
            return "error  ";
        }
    }
};
} // namespace mx::util