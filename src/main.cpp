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

static Motor motors[4] = {
    Motor(12),
    Motor(13),
    Motor(14),
    Motor(15),
};

void setup() {
    for (auto &m: motors) {
        m.init();
    }

    delay(1000);

    for (auto &m: motors) {
        for (float i = 0; i <= 1.0; i += 0.01) {
            m.write(i);
            delay(10);
        }
        m.write(0);
        delay(500);
    }

}

void loop() {}
