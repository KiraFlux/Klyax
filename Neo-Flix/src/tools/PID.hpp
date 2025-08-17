#pragma once

#include "filters.hpp"
#include "Arduino.h"


struct PID {

public:

    struct Settings {
        float p, i, d, i_limit;
    };

    const Settings &settings;

private:

    LowFrequencyFilter<float> dx_filter;
    float dx{0};
    float ix{0};
    float last_error{0};

public:

    explicit PID(const Settings &settings, float dx_filter_alpha = 1.0) :
        settings{settings}, dx_filter{dx_filter_alpha} {}

    float calc(float error, float dt) {
        if (dt > 0) {
            if (settings.p != 0) {
                ix += error * dt;
                ix = constrain(ix, -settings.i_limit, settings.i_limit);
            }

            if (settings.d != 0) {
                dx = dx_filter.calc((error - last_error) / dt);
            }
        }
        last_error = error;

        return settings.p * error + settings.i * ix + settings.d * dx;
    }

    void reset() {
        dx = 0;
        ix = 0;
        last_error = 0;
    }
};