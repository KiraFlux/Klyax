#include "PID.hpp"

#define Logger_level Logger_level_debug

#include <Arduino.h>
#include <WiFi.h>
#include "espnow/Protocol.hpp"


#include "DroneFrameDriver.hpp"
#include "PacketTimeoutManager.hpp"
#include "EasyImu.hpp"


struct DroneControl {
    /// YAW
    /// [-1.0 .. 1.0]
    /// Поворот в плоскости XY
    /// Канал пульта: left_x
    float yaw;

    /// THRUST
    /// [10.0 .. 1.0]
    /// Смещение по оси Z
    /// Канал пульта: left_y
    float thrust;

    /// ROLL
    /// [-1.0 .. 1.0]
    /// Смещение по оси Y
    /// Канал пульта: right_x
    float roll;

    /// PITCH
    /// [-1.0 .. 1.0]
    /// Смещение по оси X
    /// Канал пульта: right_y
    float pitch;

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

static

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

    control.yaw = DJC.left_x;
    control.thrust = DJC.left_y;
    control.roll = -DJC.right_x;
    control.pitch = -DJC.right_y;
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
    constexpr auto loop_frequency_hz = 100;
    constexpr auto loop_period_ms = 1000 / loop_frequency_hz;
    constexpr float dt = loop_period_ms * 1e-3;

    delay(loop_period_ms);

//    if (timeout_manager.expired()) {
//        control.thrust = max(0.0f, control.thrust - 0.02f);
//
//        control.yaw = 0;
//        control.pitch = 0;
//        control.roll = 0;
//    }
//
//    frame_driver.mixin(control.thrust, control.roll, control.pitch, control.yaw);

    const auto data = imu.read(dt);

    if (data.some()) {
        Logger_debug(
            "A[%+.2f %+.2f %+.2f]\tR[%+3.1f %+3.1f %+3.1f]",
            data.value.linear_acceleration.x, data.value.linear_acceleration.y, data.value.linear_acceleration.z,
            degrees(data.value.orientation.x), degrees(data.value.orientation.y), degrees(data.value.orientation.z)
        );
    }

}