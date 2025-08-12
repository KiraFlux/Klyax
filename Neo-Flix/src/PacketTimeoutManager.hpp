#pragma once

#include "Arduino.h"


struct PacketTimeoutManager final {

    /// Таймаут в миллисекундах
    uint32_t timeout_ms;

    /// Время следующего таймаута
    uint32_t next_timeout{0};

    explicit PacketTimeoutManager(uint32_t timeout_durations_ms) :
        timeout_ms{timeout_durations_ms} {}

    /// Обновление таймаута
    void update() {
        next_timeout = millis() + timeout_ms;
    }

    /// Проверка истечения таймаута
    inline bool expired() const {
        return millis() >= next_timeout;
    }
};
