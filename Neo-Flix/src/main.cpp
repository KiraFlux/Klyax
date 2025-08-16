

#define Logger_level Logger_level_debug

#include <Arduino.h>
#include <WiFi.h>
#include "espnow/Protocol.hpp"


#include "DroneFrameDriver.hpp"
#include "PacketTimeoutManager.hpp"
#include "EasyImu.hpp"


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
    /// [10.0 .. 1.0]
    /// Смещение по оси Z
    /// Канал пульта: left_y
    float thrust;

    /// Включено
    bool armed;
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

void onEspNowMessage(const espnow::Mac &mac, const void *data, rs::u8 size) {

    struct DualJotControlPacket {
        float left_x;
        float left_y;
        float right_x;
        float right_y;

        bool mode_toggle;
        bool mode_hold;
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
    control.roll_power = -DJC.right_x;
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

    if (not initEspNow()) { fatal(); }

    imu.gyro_bias = {0.3625f, -0.2602f, -1.5634};
    imu.accel_bias = {-13.6719f, -17.8223f, -2.9297f};
    imu.accel_scale = {+0.0010f, +0.0010f, +0.0010f};

//    imu.calibrateGyro(1000);
//    imu.calibrateAccel(1000);

    Logger_info("Start!");
}

/// Хронометр
struct Chronometer {

private:

    /// Время предыдущего измерения
    decltype(micros()) last_us{micros()};

public:

    /// Рассчитать дельту между вызовами
    /// Сек.
    float calc() noexcept {
        const auto current_us = micros();
        const auto delta_us = current_us - last_us;
        last_us = current_us;
        return static_cast<decltype(calc())>(delta_us) * 1e-6f;
    }
};

void loop() {

    static Chronometer chronometer;

    const auto dt = chronometer.calc();

    const auto ned = imu.read(dt);

    constexpr auto samples = 1000;

    static uint32_t tick = 0;
    if (tick >= samples) {
        tick = 0;

        Logger_info(
            "A[%+.2f %+.2f %+.2f]\t"
            "R[%+3.1f %+3.1f %+3.1f]\t"
            "G[%+3.1f %+3.1f %+3.1f]",
            ned.linear_acceleration.x, ned.linear_acceleration.y, ned.linear_acceleration.z,
            ned.roll() * RAD_TO_DEG, ned.pitch() * RAD_TO_DEG, ned.yaw() * RAD_TO_DEG,
            ned.rollVelocity() * RAD_TO_DEG, ned.pitchVelocity() * RAD_TO_DEG, ned.yawVelocity() * RAD_TO_DEG
        );
    }
    tick += 1;

}