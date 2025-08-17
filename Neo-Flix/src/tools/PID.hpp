#pragma once

#include <cmath>
#include "Arduino.h"
#include "tools/filters.hpp"


/// Реализация PID регулятора с внешней зависимостью настроек
struct PID {

public:

    struct Settings {
        float p, i, d, i_limit;
        float output_min, output_max;
    };

private:


    const Settings &settings;
    LowFrequencyFilter<float> dx_filter;
    float dx{0};
    float ix{0};
    float last_error{NAN};

public:

    explicit PID(const Settings &settings, float dx_filter_alpha = 1.0f) :
        settings{settings}, dx_filter{dx_filter_alpha} {}

    float calc(float error, float dt) {
        if (dt <= 0 or dt > 0.1f) {
            return 0;
        }

        if (settings.i != 0) {
            ix += error * dt;
            ix = constrain(ix, -settings.i_limit, settings.i_limit);
        }

        if (settings.d != 0 and not std::isnan(last_error)) {
            dx = dx_filter.calc((error - last_error) / dt);
        } else {
            dx = 0;
        }
        last_error = error;

        const float output = settings.p * error + settings.i * ix + settings.d * dx;
        return constrain(output, settings.output_min, settings.output_max);
    }

    void reset() {
        dx = 0;
        ix = 0;
        last_error = NAN;
    }

};

