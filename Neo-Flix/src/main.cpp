#define Logger_level Logger_level_debug

#include <Arduino.h>
#include <WiFi.h>
#include "espnow/Protocol.hpp"


#include "DroneFrameDriver.hpp"
#include "PacketTimeoutManager.hpp"


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
};

static constexpr espnow::Mac control_mac = {0x78, 0x1c, 0x3c, 0xa4, 0x96, 0xdc};

static DroneControl control;

static PacketTimeoutManager timeout_manager{200};

DroneFrameDriver frame_driver{
    .motors={
        Motor(12),
        Motor(13),
        Motor(14),
        Motor(15),
    }
};

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
}

bool initEspNow() {
    Logger_info("ESPNOW Init");

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

    Logger_debug("ESPNOW Success");
    return true;
}

void setup() {
    Serial.begin(115200);

    Logger::instance().write_func = [](const char *message, size_t length) {
        Serial.write(message, length);
    };

    frame_driver.init();

    if (not initEspNow()) {
        Logger_fatal("Espnow init error. Reboot in 5 sec");
        delay(5000);
        ESP.restart();
    }

    Logger::instance().write_func = [](const char *message, size_t length) {
        espnow::Protocol::send(control_mac, message, length);
    };

    Logger_info("Start!");
}

void safetyCheck() {
    if (timeout_manager.expired()) {
        control.thrust = max(0.0f, control.thrust - 0.02f);

        control.yaw = 0;
        control.pitch = 0;
        control.roll = 0;
    }
}

void loop() {
    constexpr auto loop_hz = 100;
    constexpr auto ms_delay = 1000 / loop_hz;

    safetyCheck();

    frame_driver.mixin(control.thrust, control.roll, control.pitch, control.yaw);

    delay(ms_delay);
}