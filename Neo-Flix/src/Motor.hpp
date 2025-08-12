#pragma once

#include <Arduino.h>


struct Motor final {

    static constexpr auto pwm_frequency = 78000;
    static constexpr auto pwm_resolution = 10;

private:

    const uint8_t pin;

public:

    constexpr explicit Motor(uint8_t pin) noexcept:
        pin{pin} {}

    void init() const noexcept {
        ledcSetup(pin, pwm_frequency, pwm_resolution);
        ledcAttachPin(pin, pin);
    }

    void write(float value) const {
        ledcWrite(pin, calcDuty(value));
    }

private:

    static uint32_t calcDuty(float value) {
        constexpr auto max_pwm = (1 << pwm_resolution) - 1;
        value = constrain(value, 0.0f, 1.0f);
        return value * max_pwm;
    }
};
