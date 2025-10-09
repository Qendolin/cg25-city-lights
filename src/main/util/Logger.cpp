#include "Logger.h"

#include <assert.h>
#include <filesystem>
#include <iostream>
#include <source_location>

std::string Logger::shortFileName(std::string file_name) {
    std::replace(file_name.begin(), file_name.end(), '\\', '/');
    size_t start = file_name.find("/src/");
    if (start == std::string::npos)
        start = 0;
    else
        start += 1;
    return file_name.substr(start);
}

std::string_view Logger::shortFunctionName(std::string_view function_name) {
    size_t start = 0;
    size_t end = function_name.length();
    for (size_t i = start; i < end; i++) {
        if (function_name[i] == ' ')
            start = i + 1;
        if (function_name[i] == '(')
            end = i;
    }
    if (end - start == 0) {
        static constexpr std::string_view anon = "<anonymous>";
        return anon;
    }
    return function_name.substr(start, end - start);
}

void Logger::info(std::string_view message, std::source_location location) {
    std::clog << "[LOG " << shortFileName(location.file_name()) << ':' << location.line() << "]: " << message << std::endl;
}

void Logger::debug(std::string_view message, std::source_location location) {
    std::string func_name = location.function_name();
    std::clog << "[DBG " << shortFileName(location.file_name()) << ':' << location.line() << ':'
              << shortFunctionName(location.function_name()) << "]: " << message << std::endl;
}

void Logger::warning(std::string_view message, std::source_location location) {
    std::clog << "[\u001B[33mWRN\u001B[0m " << shortFileName(location.file_name()) << ':' << location.line()
              << "]: " << message << std::endl;
}

void Logger::error(std::string_view message, std::source_location location) {
    std::clog << "[ERR " << shortFileName(location.file_name()) << ':' << location.line() << ':'
              << shortFunctionName(location.function_name()) << "]: " << message << std::endl;
}

void Logger::check(bool be_true, std::string_view message, std::source_location location) {
    if (be_true == true)
        return;
    std::clog << "[CHK " << shortFileName(location.file_name()) << ':' << location.line() << ':'
              << shortFunctionName(location.function_name()) << "]: " << message << std::endl;
#ifndef NDEBUG
    assert(false);
#endif
}

[[noreturn]] void Logger::fatal(std::string_view message, const cpptrace::stacktrace &trace) {
    throw std::runtime_error(std::format("FATAL: {}\n{}", message, trace.to_string(true)));
}
