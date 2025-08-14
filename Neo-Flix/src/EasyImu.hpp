#pragma once

#include <Arduino.h>
#include <ICM_20948.h>
#include <cmath>

#include "ela/vec3.hpp"

#include "filters.hpp"
#include "Logger.hpp"


struct EasyImu final {

public:

    static constexpr float one_g = 9.80665f;

    ela::vec3f gyro_bias{};
    ela::vec3f accel_bias{};
    ela::vec3f accel_scale{1.0f, 1.0f, 1.0f};

private:

    LowFrequencyFilter<ela::vec3f> accel_filter{0.2f};
    ComplementaryFilter<float> roll_filter{0.98f};
    ComplementaryFilter<float> pitch_filter{0.98f};
    ComplementaryFilter<float> yaw_filter{0.98f};

    ICM_20948_SPI imu;

    bool first_read{true};

public:

    bool init(
        gpio_num_t sck,
        gpio_num_t miso,
        gpio_num_t mosi,
        gpio_num_t cs
    ) noexcept {
        Logger_info("init");
        SPI.begin(sck, miso, mosi, cs);

        imu.begin(cs, SPI);

        if (imu.status != ICM_20948_Stat_Ok) {
            Logger_error("EasyImu init fail");
            return false;
        }

        ICM_20948_fss_t fss;
        fss.g = dps2000;
        imu.setFullScale(ICM_20948_Internal_Gyr, fss);

        Logger_debug("success");
        return true;
    }

    void calibrateGyro(int samples) noexcept {
        Logger_info("start");

        const auto s = static_cast<float>(samples);

        ela::vec3f gyro_sum;

        for (int i = 0; i < samples; i++) {
            if (imu.dataReady()) {
                imu.getAGMT();

                gyro_sum.x += imu.gyrX();
                gyro_sum.y += imu.gyrY();
                gyro_sum.z += imu.gyrZ();
            }

            delay(10);
        }

        gyro_bias.x = gyro_sum.x / s;
        gyro_bias.y = gyro_sum.y / s;
        gyro_bias.z = gyro_sum.z / s;

        Logger_debug("End");
        Logger_debug("Gyro bias: %.4f %.4f %.4f", gyro_bias.x, gyro_bias.y, gyro_bias.z);
    }

    void calibrateAccel(int samples) noexcept {
        Logger_info("Calibrating accelerometer with %d samples", samples);

        constexpr auto inf = std::numeric_limits<float>::infinity();
        ela::vec3f accel_max(-inf, -inf, -inf);
        ela::vec3f accel_min(inf, inf, inf);

        constexpr int orientations_total = 6;
        constexpr int orientation_change_timeout_ms = 8000;

        constexpr const char *orientation_names[orientations_total] = {
            "level",
            "upside down",
            "nose up",
            "nose down",
            "left",
            "right"
        };

        for (int calib_orientation = 0; calib_orientation < orientations_total; calib_orientation += 1) {
            Logger_info("%d/%d place %s", calib_orientation + 1, orientations_total, orientation_names[calib_orientation]);
            delay(orientation_change_timeout_ms);

            Logger_info("Start");

            for (int i = 0; i < samples; i++) {
                if (imu.dataReady()) {
                    imu.getAGMT();

                    const float ax = imu.accX();
                    const float ay = imu.accY();
                    const float az = imu.accZ();

                    accel_min.x = std::min(accel_min.x, ax);
                    accel_min.y = std::min(accel_min.y, ay);
                    accel_min.z = std::min(accel_min.z, az);

                    accel_max.x = std::max(accel_max.x, ax);
                    accel_max.y = std::max(accel_max.y, ay);
                    accel_max.z = std::max(accel_max.z, az);
                }
                delay(10);
            }
        }

        accel_bias = (accel_min + accel_max) * 0.5f;

        // Исправленный расчет масштабных коэффициентов
        accel_scale.x = 2.0f / (accel_max.x - accel_min.x);
        accel_scale.y = 2.0f / (accel_max.y - accel_min.y);
        accel_scale.z = 2.0f / (accel_max.z - accel_min.z);

        Logger_info("complete");
        Logger_debug("Accel bias: %+.4f %+.4f %+.4f",
                     accel_bias.x, accel_bias.y, accel_bias.z);
        Logger_debug("Accel scale: %+.4f %+.4f %+.4f",
                     accel_scale.x, accel_scale.y, accel_scale.z);
    }

    /// Система координат NED (North East Down | Вперёд Вправо Вниз)
    struct NedCoordinateSystem {
        /// Углы поворота в Рад:
        /// X: Roll (Крен) - Поворот вокруг оси X (вперёд)
        /// Y: Pitch (Тангаж) - Поворот вокруг оси Y (вправо)
        /// Z: Yaw (Рыскание) - Поворот вокруг оси Z (вниз)
        ela::vec3f orientation;

        /// Угловые скорости в Рад/с
        /// X: Roll (Крен) - Вращение вокруг оси X (вперёд)
        /// Y: Pitch (Тангаж) - Вращение вокруг оси Y (вправо)
        /// Z: Yaw (Рыскание) - Вращение вокруг оси Z (вниз)
        ela::vec3f angular_velocity;

        /// Линейное ускорениe в G
        /// X: North (вперёд)
        /// Y: East (Вправо)
        /// Z: Down (Вниз)
        ela::vec3f linear_acceleration;

        /// Roll (Крен)
        /// Поворот вокруг оси X (вперёд)
        /// Радианы
        inline float roll() const { return orientation.x; }

        /// Roll (Крен)
        /// Вращение вокруг оси X (вперёд)
        /// Радианы / секунду
        inline float rollVelocity() const { return angular_velocity.x; }

        /// Pitch (Тангаж)
        /// Поворот вокруг оси Y (вправо)
        /// Радианы
        inline float pitch() const { return orientation.y; }

        /// Pitch (Тангаж)
        /// Вращение вокруг оси Y (вправо)
        /// Радианы / секунду
        inline float pitchVelocity() const { return angular_velocity.y; }

        /// Yaw (Рыскание)
        /// Поворот вокруг оси Z (вниз)
        /// Радианы
        inline float yaw() const { return orientation.z; }

        /// Yaw (Рыскание)
        /// Вращение вокруг оси Z (вниз)
        /// Радианы / секунду
        inline float yawVelocity() const { return angular_velocity.z; }
    };

    rs::Option<NedCoordinateSystem> read(float dt) noexcept {
        constexpr float deg_to_rad = M_PI / 180.0f;

        if (!imu.dataReady()) { return {}; }

        imu.getAGMT();

        // Чтение и преобразование данных гироскопа (ИСПРАВЛЕНЫ ЗНАКИ)
        const ela::vec3f gyro{
            (imu.gyrY() - gyro_bias.y) * deg_to_rad,   // Roll (крен) - БЕЗ МИНУСА
            (imu.gyrX() - gyro_bias.x) * deg_to_rad,   // Pitch (тангаж) - БЕЗ МИНУСА
            (imu.gyrZ() - gyro_bias.z) * deg_to_rad    // Yaw (рыскание)
        };

        // Чтение и преобразование данных акселерометра
        const ela::vec3f accel = accel_filter.calc(
            {
                -(imu.accY() - accel_bias.y) * accel_scale.y,  // X: вперёд
                -(imu.accX() - accel_bias.x) * accel_scale.x,  // Y: вправо
                (imu.accZ() - accel_bias.z) * accel_scale.z    // Z: вниз
            }
        );

        // Вычисляем углы по акселерометру (ИСПРАВЛЕНЫ ЗНАКИ)
        const float roll_acc = atan2f(accel.y, accel.z);
        const float pitch_acc = -atan2f(accel.x, accel.z);

        // Инициализация фильтров при первом вызове
        if (first_read) {
            first_read = false;
            roll_filter.filtered = roll_acc;
            pitch_filter.filtered = pitch_acc;
            yaw_filter.filtered = 0;
        }

        return NedCoordinateSystem{
            .orientation = {
                normalizeAngle(roll_filter.calc(roll_acc, gyro.x, dt)),
                normalizeAngle(pitch_filter.calc(pitch_acc, gyro.y, dt)),
                normalizeAngle(yaw_filter.calc(0, -gyro.z, dt))
            },
            .angular_velocity = gyro,
            .linear_acceleration = accel
        };
    }

private:

    inline static float normalizeAngle(float angle) noexcept {
        angle = static_cast<float>(std::fmod(angle + M_PI, M_TWOPI));
        return static_cast<float>(angle >= 0 ? angle - M_PI : angle + M_PI);
    }

};
