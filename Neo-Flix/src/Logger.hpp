#pragma once

#include "Arduino.h"


struct Logger {

    using WriteFunction = void (*)(const char *, size_t);

    WriteFunction write_func{nullptr};

    static Logger &instance() {
        static Logger instance;
        return instance;
    }

    void log(const char *level, const char *file, int line, const char *msg) const {
        if (write_func == nullptr) { return; }

        char buffer[128];
        size_t len = snprintf(buffer, sizeof(buffer),
                              "[%lu][%s][%s:%d] %s\n",
                              millis(), level, file, line, msg);

        if (len > 0) {
            write_func(buffer, min(len, sizeof(buffer) - 1));
        }

    }

//    template<typename... Args> void log(const char *level, const char *file, int line, const char *format, Args... args) {
//        if (write_func == nullptr) { return; }
//
//        char buffer[128];
//        char *ptr = buffer;
//
//        // Форматируем префикс
//        size_t prefix_len = snprintf(ptr, sizeof(buffer),
//                                     "[%lu][%s][%s:%d] ",
//                                     millis(), level, file, line);
//
//        if (prefix_len >= sizeof(buffer)) { return; }
//        ptr += min(prefix_len, sizeof(buffer) - 1);
//
//        // Форматируем основное сообщение
//        size_t msg_len = snprintf(ptr, sizeof(buffer) - prefix_len, format, args...);
//        if (msg_len <= 0) { return; }
//
//        // Добавляем перевод строки
//        size_t total_len = prefix_len + msg_len;
//        if (total_len < sizeof(buffer) - 1) {
//            buffer[total_len] = '\n';
//            total_len++;
//        }
//
//        write_func(buffer, min(total_len, sizeof(buffer) - 1));
//    }

private:

    Logger() = default;
};

/// Debug
#define Logger_level_debug 0
/// Info
#define Logger_level_info 1
/// Warn
#define Logger_level_warn 2
/// Error
#define Logger_level_error 3
/// Fatal
#define Logger_level_fatal 4
/// None (Disable)
#define Logger_level_none 5


#if not defined(Logger_level)
#define Logger_level Logger_level_debug
#endif

#if Logger_level_debug >= Logger_level
#define Logger_debug(...)   Logger::instance().log("Debug",   __FILE__, __LINE__, __VA_ARGS__)
#else
#define Logger_debug(...)
#endif

#if Logger_level_info >= Logger_level
#define Logger_info(...)   Logger::instance().log("Info",   __FILE__, __LINE__, __VA_ARGS__)
#else
#define Logger_info(...)
#endif

#if Logger_level_warn >= Logger_level
#define Logger_warn(...)   Logger::instance().log("Warn",   __FILE__, __LINE__, __VA_ARGS__)
#else
#define Logger_warn(...)
#endif

#if Logger_level_error >= Logger_level
#define Logger_error(...)   Logger::instance().log("Error",   __FILE__, __LINE__, __VA_ARGS__)
#else
#define Logger_error(...)
#endif

#if Logger_level_fatal >= Logger_level
#define Logger_fatal(...)   Logger::instance().log("Fatal",   __FILE__, __LINE__, __VA_ARGS__)
#else
#define Logger_fatal(...)
#endif


