#include "PID.hpp"


#define Logger_level Logger_level_debug

#include <Arduino.h>
#include <WiFi.h>
#include "espnow/Protocol.hpp"


#include "DroneFrameDriver.hpp"
#include "PacketTimeoutManager.hpp"
#include "EasyImu.hpp"
#include "ela/vec2.hpp"


struct DroneControl {
    static constexpr float tilt_max_rad = M_PI / 6.0f;
    static constexpr float yaw_to_rad = 1.0f * M_PI;

    /// YAW
    /// [-1.0 .. 1.0]
    /// Поворот в плоскости XY
    /// Канал пульта: left_x
    float yaw_power;

    /// THRUST
    /// [10.0 .. 1.0]
    /// Смещение по оси Z
    /// Канал пульта: left_y
    float thrust;

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

    /// Включено
    bool armed;

    /// Roll (Крен) - Поворот вокруг оси X (вперёд)
    inline float roll() const {
        return roll_power * tilt_max_rad;
    }

    /// Pitch (Тангаж) - Поворот вокруг оси Y (вправо)
    inline float pitch() const {
        return pitch_power * tilt_max_rad;
    }

    /// Yaw (Рыскание) - Поворот вокруг оси Z (вниз)
    inline float yaw() const {
        return yaw_power * yaw_to_rad;
    }

};

static constexpr espnow::Mac control_mac = {0x78, 0x1c, 0x3c, 0xa4, 0x96, 0xdc};

static DroneControl control;

static PacketTimeoutManager timeout_manager{200};

static DroneFrameDriver frame_driver{
    .motors={
        Motor(12),
        Motor(13),
        Motor(14),
        Motor(15),
    }
};

static EasyImu imu;

static PID::Settings roll_and_pitch_pid_settings{
    .p = 6.0f, .i = 0.0f, .d = 0.0f, .i_limit = 0.0f
};

static PID::Settings yaw_pid_settings{
    .p = 3.0f, .i = 0.0f, .d = 0.0f, .i_limit = 0.0f
};

static PID::Settings roll_and_pitch_rate_pid_settings{
    .p = 0.05f, .i = 0.2f, .d = 0.001f, .i_limit = 0.3f
};

static PID::Settings yaw_rate_pid_settings{
    .p = 0.3f, .i = 0.0f, .d = 0.0f, .i_limit = 0.3f
};


static PID roll_pid{roll_and_pitch_pid_settings};

static PID pitch_pid{roll_and_pitch_pid_settings};

static PID yaw_pid{yaw_pid_settings};

static PID roll_rate_pid{roll_and_pitch_rate_pid_settings, 0.2f};

static PID pitch_rate_pid{roll_and_pitch_rate_pid_settings, 0.2f};

static PID yaw_rate_pid{yaw_rate_pid_settings, 1.0f};


void onEspNowMessage(const espnow::Mac &mac, const void *data, rs::u8 size) {

    struct DualJotControlPacket {
        float left_x;
        float left_y;
        float right_x;
        float right_y;

        bool toggle_left;
        bool toggle_right;
        bool hold_left;
        bool hold_right;
    };

    if (mac != control_mac) {
        Logger_warn("got message from unknown device");
        return;
    }

    if (size != sizeof(DualJotControlPacket)) {
        Logger_warn("invalid packet size");
        return;
    }

    timeout_manager.update();

    const auto &DJC = *reinterpret_cast<const DualJotControlPacket *>(data);

    control.yaw_power = DJC.left_x;
    control.thrust = DJC.left_y;
    control.roll_power = DJC.right_x;
    control.pitch_power = -DJC.right_y;
    control.armed = DJC.toggle_left;
}

static bool initEspNow() {
    Logger_info("init");

    const bool wifi_ok = WiFiClass::mode(WIFI_MODE_STA);
    if (not wifi_ok) {
        return false;
    }

    const auto init_result = espnow::Protocol::init();
    if (init_result.fail()) {
        Logger_error(rs::toString(init_result.error));
        return false;
    }

    const auto peer_result = espnow::Peer::add(control_mac);
    if (peer_result.fail()) {
        Logger_error(rs::toString(peer_result.error));
        return false;
    }

    const auto handler_result = espnow::Protocol::instance().setReceiveHandler(onEspNowMessage);
    if (handler_result.fail()) {
        Logger_error(rs::toString(handler_result.error));
        return false;
    }

    Logger_debug("success");
    return true;
}

static void fatal() {
    delay(5000);
    ESP.restart();
}

void setup() {
    Serial.begin(115200);

    Logger::instance().write_func = [](const char *message, size_t length) {
        Serial.write(message, length);
    };

    frame_driver.init();

    if (not imu.init(GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_5)) { fatal(); }

    imu.gyro_bias = {0.5354f, -0.2246f, -1.4703};
    imu.accel_bias = {-11.7188f, -13.1836f, -5.3711f};
    imu.accel_scale = {+0.0010f, +0.0010f, +0.0010f};

//    imu.calibrateGyro(1000);
//    imu.calibrateAccel(1000);

    if (not initEspNow()) { fatal(); }

//    Logger::instance().write_func = [](const char *message, size_t length) {
//        espnow::Protocol::send(control_mac, message, length);
//    };

    Logger_info("Start!");
}

void loop() {
    constexpr auto loop_frequency_hz = 500;
    constexpr auto loop_period_ms = 1000 / loop_frequency_hz;
    constexpr float dt = loop_period_ms * 1e-3;

    delay(loop_period_ms);

    if (timeout_manager.expired()) {
        control.thrust = 0;
        control.yaw_power = 0;
        control.pitch_power = 0;
        control.roll_power = 0;
        control.armed = false;
    }

    if (control.armed) {
        const auto data = imu.read(dt);

        if (data.some()) {
            const auto &ned = data.value;

            // 1. Рассчитываем ошибки ориентации
            const float roll_error = control.roll() - ned.roll();
            const float pitch_error = control.pitch() - ned.pitch();
            const float yaw_error = control.yaw() - ned.yaw();

            // 2. Внешний контур: углы -> угловые скорости
            const float roll_rate_target = roll_pid.calc(roll_error, dt);
            const float pitch_rate_target = pitch_pid.calc(pitch_error, dt);
            const float yaw_rate_target = yaw_pid.calc(yaw_error, dt);

            // 3. Рассчитываем ошибки угловых скоростей
            const float roll_rate_error = roll_rate_target - ned.rollVelocity();
            const float pitch_rate_error = pitch_rate_target - ned.pitchVelocity();
            const float yaw_rate_error = yaw_rate_target - ned.yawVelocity();

            // 4. Внутренний контур: угловые скорости -> управляющие моменты
            const float roll_torque = roll_rate_pid.calc(roll_rate_error, dt);
            const float pitch_torque = pitch_rate_pid.calc(pitch_rate_error, dt);
            const float yaw_torque = yaw_rate_pid.calc(yaw_rate_error, dt);

            frame_driver.mixin(control.thrust, roll_torque, pitch_torque, yaw_torque);

            static uint32_t next_log_ms = 0;
            if (millis() > next_log_ms) {
                next_log_ms = millis() + 100;
                Logger_debug(
                    "[err|rate|out]"
                    "Roll: [%.1f %.1f/s %.2f]\t"
                    "Pitch: [%.1f %.1f/s %.2f]\t"
                    "Yaw: [%.1f %.1f/s %.2f]",
                    degrees(roll_error), degrees(roll_rate_error), roll_torque,
                    degrees(pitch_error), degrees(pitch_rate_error), pitch_torque,
                    degrees(yaw_error), degrees(yaw_rate_error), yaw_torque
                );
            }
        }
    } else {
        frame_driver.disable();
        roll_pid.reset();
        pitch_pid.reset();
        yaw_pid.reset();
        roll_rate_pid.reset();
        pitch_rate_pid.reset();
        yaw_rate_pid.reset();
    }

}