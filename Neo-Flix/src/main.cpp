#define Logger_level Logger_level_debug

#include <Arduino.h>
#include "NFLogger.hpp"

#include "EasuImu2.hpp"


static EasyImu imu{ICM20948{SPI}};

struct DroneControl {
    /// ROLL
    /// [-1.0 .. 1.0]
    /// Смещение по оси Y
    /// Канал пульта: right_x
    float roll_power;

    /// PITCH
    /// [-1.0 .. 1.0]
    /// Смещение по оси X
    /// Канал пульта: right_y
    float pitch_power;

    /// YAW
    /// [-1.0 .. 1.0]
    /// Поворот в плоскости XY
    /// Канал пульта: left_x
    float yaw_power;

    /// THRUST
    /// [-1.0 .. 1.0]
    /// Смещение по оси Z
    /// Канал пульта: left_y
    float thrust;

    /// Включено
    bool armed;
};

static void fatal() {
    Logger_fatal("Fatal Error. Reboot in 5 s");
    delay(5000);
    ESP.restart();
}

void setup() {
    Serial.begin(115200);

    Logger::instance().write_func = [](const char *message, size_t length) {
        Serial.write(message, length);
    };

    if (not imu.init()) {
        fatal();
    }

    Logger_info("Start!");
}

void loop() {
    static double last_secs = 0;
    const double now_secs = millis() * 1e-6;
    const auto dt = static_cast<float>(now_secs - last_secs);
    last_secs = now_secs;

    auto &i = imu.imu;

    i.waitForData();
    float ax, ay, az, gx, gy, gz;
    i.getAccel(ax, ay, az);
    i.getGyro(gx, gy, gz);

    static int tick = 0;
    static float dt_sum = 0;

    dt_sum += dt;
    tick += 1;

    constexpr auto samples = 1000;

    if (tick >= samples) {
        float dt_avg = dt_sum / samples;

        Logger_debug(
            "dt: %.3f\t"
            "A[%+.2f %+.2f %+.2f]\t"
            "G[%+.2f %+.2f %+.2f]",
            dt_avg,
            ax, ay, az,
            gx, gy, gz
        );

        tick = 0;
        dt_sum = 0;
    }
}