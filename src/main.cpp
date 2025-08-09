
#include <Arduino.h>
#include "EasyImu.hpp"

EasyImu imu;

void setup() {
    Serial.begin(115200);
    while (not Serial) {}
    delay(1000);

    Serial.println("Start");

    if (not imu.init(
        GPIO_NUM_18,
        GPIO_NUM_19,
        GPIO_NUM_23,
        GPIO_NUM_5)
        ) {
        while (true) {
            Serial.println("imu fail");
            delay(2000);
        }
    }

    Serial.println("calib");

    imu.calibrate(500);

    Serial.printf(
        "G[%.2f %.2f %.2f]\tA[%.2f %.2f %.2f]\n",
        imu.gyro_bias.x,
        imu.gyro_bias.y,
        imu.gyro_bias.z,
        imu.accel_bias.x,
        imu.accel_bias.y,
        imu.accel_bias.z
    );

    Serial.println("Start");

    delay(5000);
}

void loop() {
    delay(50);

    const auto data = imu.read();

    if (data.none()) {
        return;
    }

    Serial.printf(
        "G[%.2f %.2f %.2f]\tA[%.2f %.2f %.2f]\n",
        data.value.gyro.x,
        data.value.gyro.y,
        data.value.gyro.z,
        data.value.accel.x,
        data.value.accel.y,
        data.value.accel.z
    );
}