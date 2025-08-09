#pragma once

#include <Arduino.h>


struct Motor final {

private:

    static constexpr auto pwm_frequency = 78000;
    static constexpr auto pwm_resolution = 10;

    const uint8_t pin;

public:

    constexpr explicit Motor(uint8_t pin) noexcept:
        pin{pin} {}

    void init() const noexcept {
        ledcSetup(pin, pwm_frequency, pwm_resolution);
        ledcAttachPin(pin, pin);
    }

    void write(float value) const noexcept {
        ledcWrite(pin, calcDuty(constrain(value, 0, 1)));
    }

private:

    inline static uint32_t calcDuty(float value) noexcept {
        constexpr auto pwm_max = (1 << pwm_resolution) - 1;
        return static_cast<uint32_t>(value * pwm_max);
    }

};
