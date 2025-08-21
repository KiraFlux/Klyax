#include "Arduino.h"
#include "EasyImu.hpp"


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
}

ela::vec3f fromIMUtoFLU(float x, float y, float z) {
    return ela::vec3f{-y, +x, -z} * 0.001f;
}

void loop() {
    while (not imu.imu.dataReady()) {}

    imu.imu.getAGMT();

    ela::vec3f acc = fromIMUtoFLU(imu.imu.accX(), imu.imu.accY(), imu.imu.accZ());

    float acc_roll = std::atan2(acc.y, -acc.z);
    float acc_pitch = std::atan2(-acc.x, std::hypot(acc.y, acc.z));

    Serial.printf(
        "A[%+.2f %+.2f %+.2f]\t"
        "Roll: %+3.1f\tPitch: %+3.1f\n",
        acc.x, acc.y, acc.z,
        acc_roll * RAD_TO_DEG,
        acc_pitch * RAD_TO_DEG
    );

    delay(50);
}