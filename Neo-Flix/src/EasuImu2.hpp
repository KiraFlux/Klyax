#pragma once

#include <ICM20948.h>
#include "ela/vec3.hpp"
#include "NFLogger.hpp"


struct EasyImu final {


    ICM20948 imu;

    ela::vec3f accel_bias{};
    ela::vec3f accel_scale{1.0f, 1.0f, 1.0f};
    ela::vec3f gyro_bias{};

    explicit EasyImu(ICM20948 &&imu) :
        imu{imu} {}

    bool init() {
        Logger_info("init");

#define do_try(stmt) if (not (stmt)) { return false; };

        do_try(imu.begin());
        do_try(imu.setAccelRange(ICM20948::ACCEL_RANGE_2G));
        do_try(imu.setGyroRange(ICM20948::GYRO_RANGE_2000DPS));
        do_try(imu.setDLPF(ICM20948::DLPF_MAX));
        do_try(imu.setRate(ICM20948::RATE_1KHZ_APPROX));
        do_try(imu.setupInterrupt());

#undef do_try

        Logger_debug("success");
        return true;
    }

};