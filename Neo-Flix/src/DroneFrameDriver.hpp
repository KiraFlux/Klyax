#pragma once

#include "tools/Logger.hpp"
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
        Logger_info("init");

        for (auto &m: motors) {
            m.init();
            m.write(0);
        }

        Logger_debug("success");
    }

    void mixin(
        float thrust,
        float roll,
        float pitch,
        float yaw
    ) const {
        motors[FrontLeft].write(thrust + roll - pitch + yaw);
        motors[FrontRight].write(thrust - roll - pitch - yaw);
        motors[BackLeft].write(thrust + roll + pitch - yaw);
        motors[BackRight].write(thrust - roll + pitch + yaw);
    }

    void disable() const {
        for (auto &m: motors) {
            m.write(0);
        }
    }
};
