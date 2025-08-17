#define Logger_level Logger_level_debug

#include <Arduino.h>
#include <WiFi.h>

#include "espnow/Protocol.hpp"

#include "tools/Logger.hpp"
#include "tools/time.hpp"

#include "DroneFrameDriver.hpp"
#include "EasyImu.hpp"
#include "tools/PID.hpp"


struct DroneControl final {

    /// Преобразование воздействия пульта в угловую скорость
    /// Радиан / секунду
    static constexpr float power_to_angular_velocity = 3.0;

    /// ROLL
    /// [-1.0 .. 1.0]
    /// Канал пульта: right_x
    float roll_power;

    /// PITCH
    /// [-1.0 .. 1.0]
    /// Канал пульта: right_y
    float pitch_power;

    /// YAW
    /// [-1.0 .. 1.0]
    /// Канал пульта: left_x
    float yaw_power;

    /// THRUST
    /// [10.0 .. 1.0]
    /// Канал пульта: left_y
    float thrust;

    /// Включено
    bool armed;

    /// Интерпретировать pitch как угловую скорость
    inline float pitchVelocity() const { return pitch_power * power_to_angular_velocity; }

    /// Интерпретировать roll как угловую скорость
    inline float rollVelocity() const { return roll_power * power_to_angular_velocity; }

    /// Интерпретировать yaw как угловую скорость
    inline float yawVelocity() const { return yaw_power * power_to_angular_velocity; }
};

void led(bool state) {
    digitalWrite(2, state);
}

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

void onEspNowMessage(const espnow::Mac &mac, const void *data, rs::u8 size) {

    struct DualJotControlPacket {
        float left_x;
        float left_y;
        float right_x;
        float right_y;

        bool mode_toggle;
        bool mode_hold;
        bool waiting_for_remote_menu;
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
    control.armed = DJC.mode_toggle;
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
    Logger_fatal("Fatal Error. Reboot in 5s");
    delay(5000);
    ESP.restart();
}

void setup() {
    pinMode(2, OUTPUT);
    led(HIGH);

    Serial.begin(115200);

    Logger::instance().write_func = [](const char *message, size_t length) {
        Serial.write(message, length);
    };

    frame_driver.init();

    if (not imu.init(GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_5)) { fatal(); }

    if (not initEspNow()) { fatal(); }

    imu.gyro_bias = {0.3625f, -0.2602f, -1.5634};
    imu.accel_bias = {-13.6719f, -17.8223f, -2.9297f};
    imu.accel_scale = {+0.0010f, +0.0010f, +0.0010f};

    imu.calibrateGyro(5000);
//    imu.calibrateAccel(1000);

    led(LOW);
    Logger_info("Start!");
}

void slowDownValue(float &value, float k = 0.8) {
    if (std::abs(value) > 0.01) {
        value *= k;
    } else {
        value = 0;
    }
}

void loop() {
    static Chronometer chronometer;

    static PID::Settings pitch_or_roll_velocity_pid_settings{
        .p = 0.05f,
        .i = 0.01f,
        .d = 0.0002f,
        .i_limit = 0.1f,
    };

    static PID::Settings yaw_velocity_pid_settings{
        .p = 0.04f,
        .i = 0.005f,
        .d = 0.0002f,
        .i_limit = 0.1f
    };

    static PID pitch_velocity_pid{
        pitch_or_roll_velocity_pid_settings,
        0.1f,
    };

    static PID roll_velocity_pid{
        pitch_or_roll_velocity_pid_settings,
        0.1f
    };

    static PID yaw_velocity_pid{
        yaw_velocity_pid_settings,
        1.0f,
    };

    static LowFrequencyFilter<float> yaw_error_filter{0.3f};

    const auto dt = chronometer.calc();

    if (timeout_manager.expired()) {
        control.armed = false;
    }

    if (control.armed) {
        const auto ned = imu.read(dt);

        constexpr float critical_angle = 70 * DEG_TO_RAD;

        if (std::abs(ned.pitch()) > critical_angle or std::abs(ned.roll()) > critical_angle) {
            Logger_warn("Critical roll/pitch. Disarming");
            control.armed = false;
            return;
        }

        const float roll = roll_velocity_pid.calc(control.rollVelocity() - ned.rollVelocity(), dt);
        const float pitch = pitch_velocity_pid.calc(control.pitchVelocity() - ned.pitchVelocity(), dt);

        const float yaw_error = yaw_error_filter.calc(control.yawVelocity() - ned.yawVelocity());

        float yaw = yaw_velocity_pid.calc(yaw_error, dt);
        yaw = constrain(yaw, -1.0f, 1.0f);

//        Logger_debug("yaw Set: %+.2f\tActual: %+.2f\tError: %+.2f\tOutput: %+.2f", control.yawVelocity(), ned.yawVelocity(), yaw_error, yaw);

        frame_driver.mixin(
            control.thrust,
            roll,
            pitch,
            yaw
        );

    } else {
        control.thrust = 0;
        control.pitch_power = 0;
        control.yaw_power = 0;
        control.roll_power = 0;

        pitch_velocity_pid.reset();
        roll_velocity_pid.reset();
        yaw_velocity_pid.reset();
        yaw_error_filter.reset();

        frame_driver.disable();
    }
}