#pragma once

#include "Arduino.h"


struct PacketTimeoutManager final {

private:

    /// Таймаут
    uint32_t timeout_ms;
    /// Время следующего таймаута
    uint32_t next_timeout{0};

public:

    explicit PacketTimeoutManager(uint32_t timeout_durations_ms) :
        timeout_ms{timeout_durations_ms} {}

    /// Обновление таймаута
    void update() {
        next_timeout = millis() + timeout_ms;
    }

    /// Проверка истечения таймаута
    inline bool expired() const { return millis() >= next_timeout; }
};

/// Хронометр
struct Chronometer final {

private:

    /// Время предыдущего измерения
    decltype(micros()) last_us{micros()};

public:

    /// Рассчитать дельту между вызовами
    /// Сек.
    float calc() noexcept {
        const auto current_us = micros();
        const auto delta_us = current_us - last_us;
        last_us = current_us;
        return static_cast<decltype(calc())>(delta_us) * 1e-6f;
    }
};
