#pragma once

#include <Arduino.h>
#include <ICM_20948.h>
#include <cmath>

#include "ela/vec3.hpp"

#include "tools/filters.hpp"
#include "tools/Logger.hpp"


struct EasyImu final {

public:

    struct Settings {
        ela::vec3f gyro_bias;
        ela::vec3f accel_bias;
        ela::vec3f accel_scale;
    };

    struct AccelCalibrator {
        static constexpr auto samples_per_orientation = 1000;
        static constexpr auto orientations_total = 6;

        ela::vec3f accel_min{};
        ela::vec3f accel_max{};
        int samples_collected{0};
        uint8_t current_orientation{0};
        bool active{false};
        bool paused{false}; // Флаг паузы для возможности перевернуть дрон

        void onStart() {
            constexpr auto inf = std::numeric_limits<float>::infinity();
            accel_max = {-inf, -inf, -inf};
            accel_min = {inf, inf, inf};
            samples_collected = 0;
            current_orientation = 0;
            active = true;
            paused = false;
        }

        void onSample(float x, float y, float z) {
            accel_min.x = std::min(accel_min.x, x);
            accel_min.y = std::min(accel_min.y, y);
            accel_min.z = std::min(accel_min.z, z);

            accel_max.x = std::max(accel_max.x, x);
            accel_max.y = std::max(accel_max.y, y);
            accel_max.z = std::max(accel_max.z, z);

            samples_collected += 1;
        }

        void onOrientationCollected() {
            samples_collected = 0;
            current_orientation += 1;
            paused = true;
        }

        void onEnd() {
            active = false;
            paused = false;
        }

        void apply(Settings &s) const {
            s.accel_bias = (accel_min + accel_max) * 0.5f;
            s.accel_scale.x = 2.0f / (accel_max.x - accel_min.x);
            s.accel_scale.y = 2.0f / (accel_max.y - accel_min.y);
            s.accel_scale.z = 2.0f / (accel_max.z - accel_min.z);
        }
    };

private:

    Settings &settings;

    LowFrequencyFilter<ela::vec3f> accel_filter{0.2f};
    LowFrequencyFilter<ela::vec3f> gyro_filter{0.35};
    ComplementaryFilter<float> roll_filter{0.98f};
    ComplementaryFilter<float> pitch_filter{0.98f};
    float yaw{0.0f};
    AccelCalibrator accel_calibrator{};

public:

    ICM_20948_SPI imu{};

    explicit EasyImu(Settings &settings) :
        settings{settings} {}

    bool init(
        gpio_num_t sck,
        gpio_num_t miso,
        gpio_num_t mosi,
        gpio_num_t cs
    ) noexcept {
        Logger_info("init");
        SPI.begin(sck, miso, mosi, cs);

        imu.begin(cs, SPI, 7000000);

        if (imu.status != ICM_20948_Stat_Ok) {
            Logger_error("EasyImu init fail");
            return false;
        }

        ICM_20948_fss_t accel_fss;
        accel_fss.a = gpm2;
        imu.setFullScale(ICM_20948_Internal_Acc, accel_fss);

        ICM_20948_fss_t gyro_fss;
        gyro_fss.g = dps2000;
        imu.setFullScale(ICM_20948_Internal_Gyr, gyro_fss);

        imu.enableDLPF(ICM_20948_Internal_Acc, false);
        imu.enableDLPF(ICM_20948_Internal_Gyr, false);

        ICM_20948_smplrt_t sample_rate;
        sample_rate.g = 0;
        sample_rate.a = 0;
        imu.setSampleRate(ICM_20948_Internal_Gyr, sample_rate);
        imu.setSampleRate(ICM_20948_Internal_Acc, sample_rate);

        Logger_debug("success");
        return true;
    }

    void calibrateGyro(int samples) noexcept {
        Logger_info("start");

        const auto s = static_cast<float>(samples);

        ela::vec3f gyro_sum;

        for (int i = 0; i < samples; i++) {
            while (not imu.dataReady()) {}
            imu.getAGMT();

            gyro_sum.x += imu.gyrX();
            gyro_sum.y += imu.gyrY();
            gyro_sum.z += imu.gyrZ();
        }

        settings.gyro_bias.x = gyro_sum.x / s;
        settings.gyro_bias.y = gyro_sum.y / s;
        settings.gyro_bias.z = gyro_sum.z / s;

        Logger_debug("End");
        Logger_debug("Gyro bias: %.4f %.4f %.4f", settings.gyro_bias.x, settings.gyro_bias.y, settings.gyro_bias.z);
    }

    inline void startAccelCalib() { accel_calibrator.onStart(); }

    inline uint8_t getAccelCalibOrientation() const { return accel_calibrator.current_orientation; }

    inline bool isCalibratorActive() const { return accel_calibrator.active; }

    inline bool isCalibratingAccel() const { return accel_calibrator.active and not accel_calibrator.paused; }

    void resumeAccelCalib() {
        accel_calibrator.paused = false;
    }

    bool updateAccelCalib() {
        if (not imu.dataReady()) { return false; }

        imu.getAGMT();
        accel_calibrator.onSample(imu.accX(), imu.accY(), imu.accZ());

        if (accel_calibrator.samples_collected < AccelCalibrator::samples_per_orientation) { return false; }

        accel_calibrator.onOrientationCollected();

        if (accel_calibrator.current_orientation < AccelCalibrator::orientations_total) { return true; }

        accel_calibrator.apply(settings);
        accel_calibrator.onEnd();

        Logger_debug(
            "End\n"
            "Bias: %f %f %f\n"
            "Scale: %f %f %f",
            settings.accel_bias.x, settings.accel_bias.y, settings.accel_bias.z,
            settings.accel_scale.x, settings.accel_scale.y, settings.accel_scale.z
        );

        return true;
    }


    /// Система координат FLU (Forward Left Up)
    struct FLU {

        /// Углы поворота в Рад:
        /// X: Roll (Крен)
        /// Y: Pitch (Тангаж)
        /// Z: Yaw (Рыскание)
        ela::vec3f orientation;

        /// Roll (Крен)
        /// Поворот вокруг оси X
        /// Радианы
        inline float roll() const { return orientation.x; }

        /// Pitch (Тангаж)
        /// Поворот вокруг оси Y
        /// Радианы
        inline float pitch() const { return orientation.y; }

        /// Yaw (Рыскание)
        /// Поворот вокруг оси Z (Вверх)
        /// Радианы
        inline float yaw() const { return orientation.z; }

        /// Угловые скорости в Рад/с
        /// X: Roll (Крен)
        /// Y: Pitch (Тангаж)
        /// Z: Yaw (Рыскание)
        ela::vec3f angular_velocity;

        /// Roll (Крен)
        /// Вращение вокруг оси X (вперёд)
        /// Радианы / секунду
        inline float rollVelocity() const { return angular_velocity.x; }

        /// Pitch (Тангаж)
        /// Вращение вокруг оси Y (Влево)
        /// Радианы / секунду
        inline float pitchVelocity() const { return angular_velocity.y; }

        /// Yaw (Рыскание)
        /// Вращение вокруг оси Z (Вверх)
        /// Радианы / секунду
        inline float yawVelocity() const { return angular_velocity.z; }

        /// Линейное ускорениe в G
        /// X: Forward (вперёд)
        /// Y: Left (Влево)
        /// Z: Up (Вверх)
        ela::vec3f linear_acceleration;

        /// Forward (Вперед)
        /// Ускорение по оси X
        /// G * мм / с^2
        inline float forwardAcceleration() const { return linear_acceleration.x; }

        /// Left (Влево)
        /// Ускорение по оси Y
        /// G * мм / с^2
        inline float leftAcceleration() const { return linear_acceleration.y; }

        /// Up (вверх)
        /// Ускорение по оси Z
        /// G * мм / с^2
        inline float upAcceleration() const { return linear_acceleration.z; }
    };

    FLU read(float dt) noexcept {
        constexpr float deg_to_rad = M_PI / 180.0f;

        while (not imu.dataReady()) {}

        imu.getAGMT();

        const ela::vec3f gyro = gyro_filter.calc((transformToFLU(-imu.gyrX(), -imu.gyrY(), -imu.gyrZ()) - settings.gyro_bias) * deg_to_rad);

        const ela::vec3f accel = accel_filter.calc(compMul(transformToFLU(imu.accX(), imu.accY(), imu.accZ()) - settings.accel_bias, settings.accel_scale));

        const float accel_roll = std::atan2(-accel.y, -accel.z);
        const float accel_pitch = std::atan2(accel.x, std::hypot(accel.y, accel.z)); // даёт positive при nose down;
        yaw += gyro.z * dt;

        return FLU{
            .orientation = {
                normalizeAngle(roll_filter.calc(accel_roll, gyro.x, dt)),
                normalizeAngle(pitch_filter.calc(accel_pitch, gyro.y, dt)),
                yaw
            },
            .angular_velocity = gyro,
            .linear_acceleration = accel
        };
    }

private:

    static float normalizeAngle(float angle) noexcept {
        angle = static_cast<float>(std::fmod(angle + M_PI, M_TWOPI));
        return static_cast<float>(angle >= 0 ? angle - M_PI : angle + M_PI);
    }

    inline static ela::vec3f transformToFLU(float x, float y, float z) {
        return {-y, +x, -z};
    }

    static ela::vec3f compMul(const ela::vec3f &a, const ela::vec3f &b) {
        return {
            a.x * b.x,
            a.y * b.y,
            a.z * b.z
        };
    }
};


