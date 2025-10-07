#include "Arduino.h"
#include "EasyImu.hpp"
#include "tools/time.hpp"


static EasyImu::Settings imu_set{
    .gyro_bias = {},
    .accel_bias = {},
    .accel_scale = {0.0010f, 0.0010f, 0.0010f},
};

static EasyImu imu{imu_set};


void setup() {
    Serial.begin(115200);

    Logger::instance().write_func = [](const char *message, size_t length) {
        Serial.write(message, length);
    };

    delay(1000);

    while (not imu.init(GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_5)) {
        delay(1000);
    }

    imu.calibrateGyro(5000);
}

void loop() {
    static Chronometer chronometer{};

    delay(1);
    const auto dt = chronometer.calc();

    const auto flu = imu.read(dt);

    static int i = 0;
    i += 1;
    if (i == 100) {
        i = 0;
        Serial.printf(
            "A[%+1.2f %+1.2f %+1.2f]\t"
            "O[%+3.1f %+3.1f %+3.1f]\n",
            flu.forwardAcceleration(), flu.leftAcceleration(), flu.upAcceleration(),
            flu.roll() * RAD_TO_DEG, flu.pitch() * RAD_TO_DEG, flu.yaw() * RAD_TO_DEG
        );
    }
}