#pragma once

#include "ela/vec3.hpp"
#include <Arduino.h>
#include <ICM_20948.h>


struct EasyImu final {
    ela::vec3f gyro_bias{};
    ela::vec3f accel_bias{};

    ICM_20948_SPI imu;

    bool init(
        gpio_num_t sck,
        gpio_num_t miso,
        gpio_num_t mosi,
        gpio_num_t cs
    ) noexcept {
        SPI.begin(sck, miso, mosi, cs);

        imu.begin(cs, SPI);

        if (imu.status != ICM_20948_Stat_Ok) {
            return false;
        }

        ICM_20948_fss_t fss;
        fss.g = dps2000; // Диапазон ±2000 dps
        imu.setFullScale(ICM_20948_Internal_Gyr, fss);

        return true;
    }

    void calibrate(int samples) noexcept {
        const auto s = float(samples);

        ela::vec3f gyro_sum;
        ela::vec3f accel_sum;

        for (int i = 0; i < samples; i++) {
            if (imu.dataReady()) {
                imu.getAGMT();

                gyro_sum.x += imu.gyrX();
                gyro_sum.y += imu.gyrY();
                gyro_sum.z += imu.gyrZ();

                accel_sum.x += imu.accX();
                accel_sum.y += imu.accY();
                accel_sum.z += imu.accZ();
            }

            delay(10);
        }

        gyro_bias.x = gyro_sum.x / s;
        gyro_bias.y = gyro_sum.y / s;
        gyro_bias.z = gyro_sum.z / s;

        accel_bias.x = accel_sum.x / s;
        accel_bias.y = accel_sum.y / s;
        accel_bias.z = accel_sum.z / s;
    }

    struct Data {
        ela::vec3f gyro;
        ela::vec3f accel;
    };

    rs::Option<Data> read() noexcept {
        if (not imu.dataReady()) { return {}; }

        imu.getAGMT();
        return {
            {
                {
                    imu.gyrX() - gyro_bias.x,
                    imu.gyrY() - gyro_bias.y,
                    imu.gyrZ() - gyro_bias.z
                },
                {
                    imu.accX() - accel_bias.x,
                    imu.accY() - accel_bias.y,
                    imu.accZ() - accel_bias.z
                }
            }
        };

    }

};
