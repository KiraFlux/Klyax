#pragma once

#include "utils.hpp"
#include "Motor.hpp"


struct DroneFrameDriver {

    enum MotorIndex {
        /// M0 - Задний левый
        BackLeft,
        /// M1 - Задний правый
        BackRight,
        /// M2 - Передний левый
        FrontRight,
        /// M3 - Передний правый
        FrontLeft,
        /// Общее кол-во
        TotalCount
    };

    /// Конфигурация моторов (X-расположение)
    const Motor motors[MotorIndex::TotalCount];

    void init() const {
        LOG("Motors Init");

        for (auto &m: motors) {
            m.init();
            m.write(0);
        }

        LOG("Motors Success");
    }

    void mixin(
        float thrust,
        float roll,
        float pitch,
        float yaw
    ) const {
        motors[BackLeft].write(thrust - roll - pitch + yaw);
        motors[BackRight].write(thrust + roll - pitch - yaw);
        motors[FrontRight].write(thrust + roll + pitch + yaw);
        motors[FrontLeft].write(thrust - roll + pitch - yaw);
    }
};
